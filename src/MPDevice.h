/******************************************************************************
 **  Copyright (c) Raoul Hecky. All Rights Reserved.
 **
 **  Calaos is free software; you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation; either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  Calaos is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with Foobar; if not, write to the Free Software
 **  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 **
 ******************************************************************************/
#ifndef MPDEVICE_H
#define MPDEVICE_H

#include <QObject>
#include "Common.h"
#include "MooltipassCmds.h"
#include "QtHelper.h"
#include "AsyncJobs.h"
#include "MPNode.h"

typedef std::function<void(bool success, const QByteArray &data, bool &done)> MPCommandCb;

class MPCommand
{
public:
    QByteArray data;
    MPCommandCb cb;
    bool running = false;
};

class MPDevice: public QObject
{
    Q_OBJECT
    QT_WRITABLE_PROPERTY(Common::MPStatus, status)
    QT_WRITABLE_PROPERTY(int, keyboardLayout)
    QT_WRITABLE_PROPERTY(bool, lockTimeoutEnabled)
    QT_WRITABLE_PROPERTY(int, lockTimeout)
    QT_WRITABLE_PROPERTY(bool, screensaver)
    QT_WRITABLE_PROPERTY(bool, userRequestCancel)
    QT_WRITABLE_PROPERTY(int, userInteractionTimeout)
    QT_WRITABLE_PROPERTY(bool, flashScreen)
    QT_WRITABLE_PROPERTY(bool, offlineMode)
    QT_WRITABLE_PROPERTY(bool, tutorialEnabled)
    QT_WRITABLE_PROPERTY(bool, memMgmtMode)
    QT_WRITABLE_PROPERTY(int, flashMbSize)
    QT_WRITABLE_PROPERTY(QString, hwVersion)

public:
    MPDevice(QObject *parent);
    virtual ~MPDevice();

    /* Send a command with data to the device */
    void sendData(unsigned char cmd, const QByteArray &data = QByteArray(), MPCommandCb cb = [](bool, const QByteArray &, bool &){});
    void sendData(unsigned char cmd, MPCommandCb cb = [](bool, const QByteArray &, bool &){});

    void updateKeyboardLayout(int lang);
    void updateLockTimeoutEnabled(bool en);
    void updateLockTimeout(int timeout);
    void updateScreensaver(bool en);
    void updateUserRequestCancel(bool en);
    void updateUserInteractionTimeout(int timeout);
    void updateFlashScreen(bool en);
    void updateOfflineMode(bool en);
    void updateTutorialEnabled(bool en);

    //mem mgmt mode
    void startMemMgmtMode();
    void exitMemMgmtMode();

    //reload parameters from MP
    void loadParameters();

    //Send current date to MP
    void setCurrentDate();

    //Ask a password for specified service/login to MP
    void askPassword(const QString &service, const QString &login,
                     std::function<void(bool success, const QString &login, const QString &pass)> cb);

    //get 32 random bytes from device
    void getRandomNumber(std::function<void(bool success, const QByteArray &nums)> cb);

    //After successfull mem mgmt mode, clients can query data
    QList<MPNode *> &getLoginNodes() { return loginNodes; }
    QList<MPNode *> &getDataNodes() { return dataNodes; }

signals:
    /* Signal emited by platform code when new data comes from MP */
    /* A signal is used for platform code that uses a dedicated thread */
    void platformDataRead(const QByteArray &data);

    /* the command has failed in platform code */
    void platformFailed();

private slots:
    void newDataRead(const QByteArray &data);
    void commandFailed();
    void sendDataDequeue();

private:
    /* Platform function for starting a read, should be implemented in platform class */
    virtual void platformRead() {}

    /* Platform function for writing data, should be implemented in platform class */
    virtual void platformWrite(const QByteArray &data) { Q_UNUSED(data); }

    void loadLoginNode(AsyncJobs *jobs, const QByteArray &address);
    void loadLoginChildNode(AsyncJobs *jobs, MPNode *parent, const QByteArray &address);
    void loadDataNode(AsyncJobs *jobs, const QByteArray &address);
    void loadDataChildNode(AsyncJobs *jobs, MPNode *parent, const QByteArray &address);

    //timer that asks status
    QTimer *statusTimer = nullptr;

    //command queue
    QQueue<MPCommand> commandQueue;

    //Values loaded when needed (e.g. mem mgmt mode)
    QByteArray ctrValue;
    QList<QByteArray> cpzCtrValue;
    QList<QByteArray> favoritesAddrs;

    QList<MPNode *> loginNodes; //list of all parent nodes for credentials
    QList<MPNode *> dataNodes; //list of all parent nodes for data nodes
};

#endif // MPDEVICE_H
