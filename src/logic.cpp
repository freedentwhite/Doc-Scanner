/*!
 *  Doc Scanner - application for Sailfish OS smartphones developed using
 *  Qt/QML.
 *  Copyright (C) 2014 Mikko Leppänen
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <QString>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPdfWriter>
#include <QPainter>
#include <QRect>
#include <QtGlobal>
#include <QDesktopServices>
#include <QUrl>
#include <QMatrix>
#include <QDateTime>
#include <QPoint>
#include <QColor>
#include <QStandardPaths>
#include <QProcess>
#include <QRegExp>
#include <QScopedPointer>
#include <QtConcurrent/QtConcurrentRun>
#include <QTimer>
#include <QThreadPool>
#include <QFuture>
#include "logic.h"

// algorithm for sharpening the image
void enhanceImage(QImage &origin, const QString &pathToImage)
{
    QImage newImage(origin);
    int kernel[3][3] = { { 0, -3, 0 }, { -3, 50, -3 }, { 0, -3, 0 } };
    // int kernelSize = 3;
    constexpr int kernelDiv2 = 1;
    constexpr int sumKernel = 38;
    int r1, g1, b1, r2, g2, b2, r3, g3, b3, r4, g4, b4;
    QColor color;
    int N1 = (newImage.width() - kernelDiv2) / 4;
    int N2 = (newImage.height() - kernelDiv2) / 4;
    for (int x = kernelDiv2; x <= N1; ++x) {
        for (int y = kernelDiv2; y <= N2; ++y) {

            r1 = 0;
            g1 = 0;
            b1 = 0;

            for (int i = -kernelDiv2; i <= kernelDiv2; ++i) {
                for (int j = -kernelDiv2; j <= kernelDiv2; ++j) {
                    color = QColor(origin.pixel(x + i, y + j));
                    r1 += color.red() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    g1 +=
                        color.green() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    b1 += color.blue() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                }
            }
            r1 = qBound(0, r1 / sumKernel, 255);
            g1 = qBound(0, g1 / sumKernel, 255);
            b1 = qBound(0, b1 / sumKernel, 255);

            newImage.setPixel(x, y, qRgb(r1, g1, b1));

            r2 = 0;
            g2 = 0;
            b2 = 0;

            for (int i = -kernelDiv2; i <= kernelDiv2; ++i) {
                for (int j = -kernelDiv2; j <= kernelDiv2; ++j) {
                    color = QColor(origin.pixel(x + N1 + i, y + N2 + j));
                    r2 += color.red() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    g2 +=
                        color.green() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    b2 += color.blue() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                }
            }
            r2 = qBound(0, r2 / sumKernel, 255);
            g2 = qBound(0, g2 / sumKernel, 255);
            b2 = qBound(0, b2 / sumKernel, 255);

            newImage.setPixel(x + N1, y + N2, qRgb(r2, g2, b2));

            r3 = 0;
            g3 = 0;
            b3 = 0;

            for (int i = -kernelDiv2; i <= kernelDiv2; ++i) {
                for (int j = -kernelDiv2; j <= kernelDiv2; ++j) {
                    color =
                        QColor(origin.pixel(x + 2 * N1 + i, y + 2 * N2 + j));
                    r3 += color.red() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    g3 +=
                        color.green() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    b3 += color.blue() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                }
            }
            r3 = qBound(0, r3 / sumKernel, 255);
            g3 = qBound(0, g3 / sumKernel, 255);
            b3 = qBound(0, b3 / sumKernel, 255);

            newImage.setPixel(x + 2 * N1, y + 2 * N2, qRgb(r3, g3, b3));

            r4 = 0;
            g4 = 0;
            b4 = 0;

            for (int i = -kernelDiv2; i <= kernelDiv2; ++i) {
                for (int j = -kernelDiv2; j <= kernelDiv2; ++j) {
                    color = QColor(
                        origin.pixel(x + 3 * N1 + i - 1, y + 3 * N2 + j - 1));
                    r4 += color.red() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    g4 +=
                        color.green() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                    b4 += color.blue() * kernel[kernelDiv2 + i][kernelDiv2 + j];
                }
            }
            r4 = qBound(0, r4 / sumKernel, 255);
            g4 = qBound(0, g4 / sumKernel, 255);
            b4 = qBound(0, b4 / sumKernel, 255);

            newImage.setPixel(x + 3 * N1, y + 3 * N2, qRgb(r4, g4, b4));
        }
    }
    newImage.save(pathToImage);
}

Logic::Logic(QObject *parent) : QObject(parent) {}

QString Logic::scanImage(int x, int y, int w, int h, const QString &pathToImage)
{
    QFile file(pathToImage);
    if (file.exists()) {
        QImage img;
        img.load(pathToImage);

        qreal h_factor = img.height() / 540.0;
        qreal w_factor = img.width() / 960.0;

        int x_new = x >= 30 ? qRound(static_cast<qreal>(x * w_factor)) - 25
                            : qRound(static_cast<qreal>(x * w_factor));
        int y_new = y >= 30 ? qRound(static_cast<qreal>(y * h_factor)) - 25
                            : qRound(static_cast<qreal>(y * h_factor));
        int w_new = w < 930 ? qRound(static_cast<qreal>(w * w_factor)) + 25
                            : qRound(static_cast<qreal>(w * w_factor));
        int h_new = h < 510 ? qRound(static_cast<qreal>(h * h_factor)) + 25
                            : qRound(static_cast<qreal>(h * h_factor));

        QRect rect(x_new, y_new, w_new, h_new);
        img = img.copy(rect);
        QString suffix = QFileInfo(pathToImage).suffix();
        QString newImageName = pathToImage;
        QDate date = QDate::currentDate();
        QTime time = QTime::currentTime();
        if (pathToImage.contains("_scan_created_", Qt::CaseInsensitive)) {
            newImageName.truncate(newImageName.indexOf("_scan_created_"));
            newImageName = newImageName + QString("_scan_created_") +
                           date.toString("dd-MM-yyyy") + "_" +
                           time.toString("hh:mm:ss") + "." + suffix;
        } else {
            newImageName.truncate(newImageName.lastIndexOf("."));
            newImageName = newImageName + QString("_scan_created_") +
                           date.toString("dd-MM-yyyy") + "_" +
                           time.toString("hh:mm:ss") + "." + suffix;
        }
        // run enhancement function in another thread
        QFuture<void> f = QtConcurrent::run(enhanceImage, img, newImageName);
        f.waitForFinished();

        return newImageName;
    }
    return QString();
}

int Logic::getImageHeight(const QString &pathToImage) const
{
    QFile file(pathToImage);
    if (file.exists()) {
        QImage img;
        img.load(pathToImage);
        file.close();
        return img.height();
    }
    return 0;
}

QString Logic::getImageSize(const QString &pathToImage) const
{
    QFile file(pathToImage);
    if (file.exists()) {
        auto size = QFileInfo(pathToImage).size();
        if (size <= 100000) {
            return QString::number(static_cast<double>(size / 1000), 'f', 1)
                       .replace(".", ",") +
                   QString(" kB");
        } else {
            return QString::number(static_cast<double>(size / 1e6), 'f', 1)
                       .replace(".", ",") +
                   QString(" MB");
        }
    }
    return QString();
}

QString Logic::getImageCreateDate(const QString &pathToImage) const
{
    QFile file(pathToImage);
    if (file.exists()) {
        QDateTime datetime = QFileInfo(pathToImage).created();
        file.close();
        return datetime.toString("ddd MMMM d yyyy") + ", " +
               datetime.toString("hh:mm:ss");
    }
    return QString();
}

void Logic::sendEmail() const { QDesktopServices::openUrl(QUrl("mailto:")); }

void Logic::convertToPDF(const QString &pathToFile) const
{
    QString pdfFileName(pathToFile);
    pdfFileName =
        pdfFileName.replace(pdfFileName.lastIndexOf(".") + 1,
                            QFileInfo(pathToFile).suffix().length(), "pdf");
    QPdfWriter writer(pdfFileName);
    QPainter painter(&writer);
    writer.setPageSize(QPagedPaintDevice::A4);
    painter.drawPixmap(
        QRect(0, 0, writer.logicalDpiX() * 8, writer.logicalDpiY() * 10),
        QPixmap(pathToFile));
    painter.end();

    QString newPdfFileName =
        QStandardPaths::displayName(QStandardPaths::DocumentsLocation) + "/" +
        QFileInfo(pdfFileName).fileName();
    QFile file(pdfFileName);
    try { file.rename(newPdfFileName); }
    catch (const std::exception &e)
    {
        qWarning("Unable to rename file %s\n%s", qPrintable(pdfFileName),
                 qPrintable(e.what()));
    }
}

void Logic::rotateImage(const QString &pathToImage) const
{
    QFile file(pathToImage);
    if (file.exists()) {
        QImage img;
        img.load(pathToImage);
        QMatrix matrix;
        img.transformed(matrix.rotate(45.0),
                        Qt::TransformationMode::SmoothTransformation);
        file.remove();
        img.save(pathToImage);
    }
}

QString Logic::getVersion() const
{
    QScopedPointer<QObject> parent(new QObject(nullptr));
    QScopedPointer<QProcess> rpm(new QProcess(parent.data()));
    QStringList args;
    args << "-qa"
         << "--queryformat"
         << "'%{name}-%{version}-%{release}\n'";
    rpm->start("rpm", args);
    if (!rpm->waitForStarted()) {
        return QString();
    }
    if (!rpm->waitForFinished()) {
        return QString();
    }
    QString rpms(rpm->readAll());
    QString packageName = "harbour-docscanner";
    if (rpms.contains(packageName, Qt::CaseInsensitive)) {
        QRegExp rx(packageName + "-" + "(\\d+\\.\\d+)");
        rx.setCaseSensitivity(Qt::CaseInsensitive);
        int pos = rx.indexIn(rpms);
        if (pos > -1) {
            return rx.cap(1);
        }
    }
    return QString();
}

void Logic::checkIfThreadIsDone()
{
    if (QThreadPool::globalInstance()->activeThreadCount()) {
        QTimer::singleShot(100, this, SLOT(checkIfThreadIsDone()));
    }
}

int Logic::getImageWidth(const QString &pathToImage) const
{
    QFile file(pathToImage);
    if (file.exists()) {
        QImage img;
        img.load(pathToImage);
        file.close();
        return img.width();
    }
    return 0;
}
