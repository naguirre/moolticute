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
#include "MPDevice.h"

MPDevice::MPDevice(QObject *parent):
    QObject(parent)
{
    set_status(Common::UnknownStatus);
    set_memMgmtMode(false); //by default device is not in MMM

    statusTimer = new QTimer(this);
    statusTimer->start(500);
    connect(statusTimer, &QTimer::timeout, [=]()
    {
        sendData(MP_MOOLTIPASS_STATUS, [=](bool success, const QByteArray &data, bool &)
        {
            if (!success)
                return;
            if ((quint8)data.at(1) == MP_MOOLTIPASS_STATUS)
            {
                Common::MPStatus s = (Common::MPStatus)data.at(2);
                qDebug() << "received MP_MOOLTIPASS_STATUS: " << (int)data.at(2);
                if (s != get_status())
                {
                    if (s == Common::Unlocked)
                        QTimer::singleShot(10, [=]()
                        {
                            setCurrentDate();
                            loadParameters();
                        });
                }
                set_status(s);
            }
        });
    });

    connect(this, SIGNAL(platformDataRead(QByteArray)), this, SLOT(newDataRead(QByteArray)));
//    connect(this, SIGNAL(platformFailed()), this, SLOT(commandFailed()));
}

MPDevice::~MPDevice()
{
}

void MPDevice::sendData(unsigned char c, const QByteArray &data, MPCommandCb cb)
{
    MPCommand cmd;

    // Prepare MP packet
    cmd.data.append(data.size());
    cmd.data.append(c);
    cmd.data.append(data);
    cmd.cb = std::move(cb);

    commandQueue.enqueue(cmd);

    if (!commandQueue.head().running)
        sendDataDequeue();
}

void MPDevice::sendData(unsigned char cmd, MPCommandCb cb)
{
    sendData(cmd, QByteArray(), std::move(cb));
}

void MPDevice::sendDataDequeue()
{
    if (commandQueue.isEmpty())
        return;

    MPCommand &currentCmd = commandQueue.head();
    currentCmd.running = true;

    // send data with platform code
    qDebug() << "Platform send command: " << QString("0x%1").arg((quint8)currentCmd.data[1], 2, 16, QChar('0'));
    platformWrite(currentCmd.data);
}

void MPDevice::loadParameters()
{
    AsyncJobs *jobs = new AsyncJobs(this);

    jobs->append(new MPCommandJob(this,
                                  MP_VERSION,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received MP version FLASH size: " << (quint8)data.at(2) << "Mb";
        QString hw = QString(data.mid(3, (quint8)data.at(0) - 2));
        qDebug() << "received MP version hw: " << hw;
        set_flashMbSize((quint8)data.at(2));
        set_hwVersion(hw);
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, KEYBOARD_LAYOUT_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received language: " << (quint8)data.at(2);
        set_keyboardLayout((quint8)data.at(2));
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, LOCK_TIMEOUT_ENABLE_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received lock timeout enable: " << (quint8)data.at(2);
        set_lockTimeoutEnabled(data.at(2) != 0);
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, LOCK_TIMEOUT_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received lock timeout: " << (quint8)data.at(2);
        set_lockTimeout((quint8)data.at(2));
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, SCREENSAVER_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received screensaver: " << (quint8)data.at(2);
        set_screensaver(data.at(2) != 0);
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, USER_REQ_CANCEL_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received userRequestCancel: " << (quint8)data.at(2);
        set_userRequestCancel(data.at(2) != 0);
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, USER_INTER_TIMEOUT_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received userInteractionTimeout: " << (quint8)data.at(2);
        set_userInteractionTimeout((quint8)data.at(2));
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, FLASH_SCREEN_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received flashScreen: " << (quint8)data.at(2);
        set_flashScreen(data.at(2) != 0);
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, OFFLINE_MODE_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received offlineMode: " << (quint8)data.at(2);
        set_offlineMode(data.at(2) != 0);
        return true;
    }));

    jobs->append(new MPCommandJob(this,
                                  MP_GET_MOOLTIPASS_PARM,
                                  QByteArray(1, TUTORIAL_BOOL_PARAM),
                                  [=](const QByteArray &data, bool &) -> bool
    {
        qDebug() << "received tutorialEnabled: " << (quint8)data.at(2);
        set_tutorialEnabled(data.at(2) != 0);
        return true;
    }));

    connect(jobs, &AsyncJobs::finished, [=](const QByteArray &data)
    {
        Q_UNUSED(data);
        //data is last result
        //all jobs finished success
        qInfo() << "Finished loading device options";
    });

    connect(jobs, &AsyncJobs::failed, [=](AsyncJob *failedJob)
    {
        Q_UNUSED(failedJob);
        qCritical() << "Loading option failed";
    });

    jobs->start();
}

void MPDevice::commandFailed()
{
    //TODO: fix this to work as it should on all platforms
    //this must be only called once when something went wrong
    //with the current command
//    MPCommand currentCmd = commandQueue.head();
//    currentCmd.cb(false, QByteArray());
//    commandQueue.dequeue();

//    QTimer::singleShot(150, this, SLOT(sendDataDequeue()));
}

void MPDevice::newDataRead(const QByteArray &data)
{
    //we assume that the QByteArray size is at least 64 bytes
    //this should be done by the platform code

    if (commandQueue.isEmpty())
    {
        qWarning() << "Command queue is empty!";
        qWarning() << "Packet data " << " size:" << (quint8)data[0] << " data:" << data;
        return;
    }

    MPCommand currentCmd = commandQueue.head();

    bool done = true;
    currentCmd.cb(true, data, done);

    if (done)
    {
        commandQueue.dequeue();
        sendDataDequeue();
    }
}

void MPDevice::updateKeyboardLayout(int lang)
{
    QByteArray ba;
    ba.append((quint8)KEYBOARD_LAYOUT_PARAM);
    ba.append((quint8)lang);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateLockTimeoutEnabled(bool en)
{
    QByteArray ba;
    ba.append((quint8)LOCK_TIMEOUT_ENABLE_PARAM);
    ba.append((quint8)en);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateLockTimeout(int timeout)
{
    if (timeout < 0) timeout = 0;
    if (timeout > 0xFF) timeout = 0xFF;

    QByteArray ba;
    ba.append((quint8)LOCK_TIMEOUT_PARAM);
    ba.append((quint8)timeout);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateScreensaver(bool en)
{
    QByteArray ba;
    ba.append((quint8)SCREENSAVER_PARAM);
    ba.append((quint8)en);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateUserRequestCancel(bool en)
{
    QByteArray ba;
    ba.append((quint8)USER_REQ_CANCEL_PARAM);
    ba.append((quint8)en);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateUserInteractionTimeout(int timeout)
{
    if (timeout < 0) timeout = 0;
    if (timeout > 0xFF) timeout = 0xFF;

    QByteArray ba;
    ba.append((quint8)USER_INTER_TIMEOUT_PARAM);
    ba.append((quint8)timeout);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateFlashScreen(bool en)
{
    QByteArray ba;
    ba.append((quint8)FLASH_SCREEN_PARAM);
    ba.append((quint8)en);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateOfflineMode(bool en)
{
    QByteArray ba;
    ba.append((quint8)OFFLINE_MODE_PARAM);
    ba.append((quint8)en);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::updateTutorialEnabled(bool en)
{
    QByteArray ba;
    ba.append((quint8)TUTORIAL_BOOL_PARAM);
    ba.append((quint8)en);
    sendData(MP_SET_MOOLTIPASS_PARM, ba);
}

void MPDevice::startMemMgmtMode()
{
    /* Start MMM here, and load all memory data from the device
     * (favorites, nodes, etc...)
     */

    if (get_memMgmtMode()) return;

    AsyncJobs *jobs = new AsyncJobs(this);

    //Ask device to go into MMM first
    jobs->append(new MPCommandJob(this, MP_START_MEMORYMGMT,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        return (quint8)data.at(2) == 0x01;
    }));

    //Get CTR value
    jobs->append(new MPCommandJob(this, MP_GET_CTRVALUE,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        if (data[0] == 1) return false;
        ctrValue = data.mid(2, data[0]);
        qDebug() << "CTR value is: " << ctrValue;
        return true;
    }));

    //Get CPZ and CTR value
    jobs->append(new MPCommandJob(this, MP_GET_CARD_CPZ_CTR,
                                  [=](const QByteArray &data, bool &done) -> bool
    {
        if ((quint8)data[1] == MP_CARD_CPZ_CTR_PACKET)
        {
            done = false;
            QByteArray cpz = data.mid(2, data[0]);
            qDebug() << "CPZ packet: " << cpz;
            cpzCtrValue.append(cpz);
        }
        return true;
    }));

    //Add jobs for favorites
    favoritesAddrs.clear();
    for (int i = 0;i < MOOLTIPASS_FAV_MAX;i++)
    {
        jobs->append(new MPCommandJob(this, MP_GET_FAVORITE,
                                      QByteArray(1, (quint8)i),
                                      [=](const QByteArray &) -> bool
        {
            if (i == 0) qInfo() << "Loading favorites...";
            return true;
        },
                                      [=](const QByteArray &data, bool &) -> bool
        {
            if (data[0] == 1) return false;
            favoritesAddrs.append(data.mid(2, 4));
            return true;
        }));
    }

    qDeleteAll(loginNodes);
    loginNodes.clear();

    //Get parent node start address
    jobs->append(new MPCommandJob(this, MP_GET_STARTING_PARENT,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        if (data[0] == 1) return false;
        QByteArray addr = data.mid(2, data[0]);

        //if parent address is not null, load nodes
        if (addr != MPNode::EmptyAddress)
        {
            qInfo() << "Loading parent nodes...";
            loadLoginNode(jobs, addr);
        }
        else
            qInfo() << "No parent nodes to load.";

        return true;
    }));

    qDeleteAll(dataNodes);
    dataNodes.clear();

    //Get parent data node start address
    jobs->append(new MPCommandJob(this, MP_GET_DN_START_PARENT,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        if (data[0] == 1) return false;
        QByteArray addr = data.mid(2, data[0]);

        if (addr != MPNode::EmptyAddress)
        {
            qInfo() << "Loading parent data nodes...";
            loadDataNode(jobs, addr);
        }
        else
            qInfo() << "No parent data nodes to load.";

        return true;
    }));

    connect(jobs, &AsyncJobs::finished, [=](const QByteArray &data)
    {
        Q_UNUSED(data);
        //data is last result
        //all jobs finished success

        qInfo() << "Mem management mode enabled";
        force_memMgmtMode(true);
    });

    connect(jobs, &AsyncJobs::failed, [=](AsyncJob *failedJob)
    {
        Q_UNUSED(failedJob);
        qCritical() << "Setting device in MMM failed";
        force_memMgmtMode(false);
    });

    jobs->start();
}

void MPDevice::loadLoginNode(AsyncJobs *jobs, const QByteArray &address)
{
    MPNode *pnode = new MPNode(this);
    loginNodes.append(pnode);

    jobs->append(new MPCommandJob(this, MP_READ_FLASH_NODE,
                                  address,
                                  [=](const QByteArray &data, bool &done) -> bool
    {
        if ((quint8)data[0] <= 1) return false;

        pnode->appendData(data.mid(2, data[0]));

        //Continue to read data until the node is fully received
        if (!pnode->isValid())
            done = false;
        else
        {
            //Node is loaded
            qDebug() << "Parent node loaded: " << pnode->getService();
            if (pnode->getStartChildAddress() != MPNode::EmptyAddress)
            {
                qDebug() << "Loading child nodes...";
                loadLoginChildNode(jobs, pnode, pnode->getStartChildAddress());
            }
            else
                qDebug() << "Parent does not have childs.";

            //Load next parent
            if (pnode->getNextParentAddress() != MPNode::EmptyAddress)
                loadLoginNode(jobs, pnode->getNextParentAddress());
        }

        return true;
    }));
}

void MPDevice::loadLoginChildNode(AsyncJobs *jobs, MPNode *parent, const QByteArray &address)
{
    MPNode *cnode = new MPNode(this);
    parent->appendChild(cnode);

    jobs->append(new MPCommandJob(this, MP_READ_FLASH_NODE,
                                  address,
                                  [=](const QByteArray &data, bool &done) -> bool
    {
        if ((quint8)data[0] <= 1) return false;

        cnode->appendData(data.mid(2, data[0]));

        //Continue to read data until the node is fully received
        if (!cnode->isValid())
            done = false;
        else
        {
            //Node is loaded
            qDebug() << "Child node loaded: " << cnode->getLogin();

            //Load next child
            if (cnode->getNextChildAddress() != MPNode::EmptyAddress)
                loadLoginChildNode(jobs, parent, cnode->getNextChildAddress());
        }

        return true;
    }));
}

void MPDevice::loadDataNode(AsyncJobs *jobs, const QByteArray &address)
{
    MPNode *pnode = new MPNode(this);
    dataNodes.append(pnode);

    jobs->append(new MPCommandJob(this, MP_READ_FLASH_NODE,
                                  address,
                                  [=](const QByteArray &data, bool &done) -> bool
    {
        if ((quint8)data[0] <= 1) return false;

        pnode->appendData(data.mid(2, data[0]));

        //Continue to read data until the node is fully received
        if (!pnode->isValid())
            done = false;
        else
        {
            //Node is loaded
            qDebug() << "Parent data node loaded";

            //Load next parent
            if (pnode->getNextParentAddress() != MPNode::EmptyAddress)
            {
                qDebug() << "Loading data child nodes...";
                loadDataNode(jobs, pnode->getNextParentAddress());
            }
            else
                qDebug() << "Parent data node does not have childs.";
        }

        return true;
    }));
}

void MPDevice::loadDataChildNode(AsyncJobs *jobs, MPNode *parent, const QByteArray &address)
{
    MPNode *cnode = new MPNode(this);
    parent->appendChildData(cnode);

    jobs->append(new MPCommandJob(this, MP_READ_FLASH_NODE,
                                  address,
                                  [=](const QByteArray &data, bool &done) -> bool
    {
        if ((quint8)data[0] <= 1) return false;

        cnode->appendData(data.mid(2, data[0]));

        //Continue to read data until the node is fully received
        if (!cnode->isValid())
            done = false;
        else
        {
            //Node is loaded
            qDebug() << "Child data node loaded";

            //Load next child
            if (cnode->getNextChildAddress() != MPNode::EmptyAddress)
                loadDataChildNode(jobs, parent, cnode->getNextChildAddress());
        }

        return true;
    }));
}

void MPDevice::exitMemMgmtMode()
{
    if (!get_memMgmtMode()) return;

    ctrValue.clear();
    cpzCtrValue.clear();
    qDeleteAll(loginNodes);
    loginNodes.clear();
    qDeleteAll(dataNodes);
    dataNodes.clear();
    favoritesAddrs.clear();

    sendData(MP_END_MEMORYMGMT, [=](bool success, const QByteArray &data, bool &)
    {
        if (success)
        {
            qDebug() << "received MP_END_MEMORYMGMT: " << (quint8)data.at(1) << " - " << (quint8)data.at(2);
            if ((quint8)data.at(1) == MP_END_MEMORYMGMT &&
                (quint8)data.at(2) == 0x01)
                qDebug() << "Mem management mode disabled";
            else
                qWarning() << "Mem management mode disable was not ack by the device!";

            force_memMgmtMode(false);
        }
        else
            //force clients to update their status
            force_memMgmtMode(false);
    });
}

void MPDevice::setCurrentDate()
{
    //build current date payload and send to device
    QByteArray d = Common::dateToBytes(QDate::currentDate());

    qDebug() << "Sending current date: " <<
                QString("0x%1").arg((quint8)d[0], 2, 16, QChar('0')) <<
                QString("0x%1").arg((quint8)d[1], 2, 16, QChar('0'));

    sendData(MP_SET_DATE, d);
}

void MPDevice::askPassword(const QString &service, const QString &login,
                        std::function<void(bool success, const QString &login, const QString &pass)> cb)
{
    AsyncJobs *jobs = new AsyncJobs(this);

    QByteArray sdata = service.toUtf8();
    sdata.append((char)0);

    jobs->append(new MPCommandJob(this, MP_CONTEXT,
                                  sdata,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        if (data[2] != 1)
        {
            qWarning() << "Error setting context: " << (quint8)data[2];
            return false;
        }
        return true;
    }));

    jobs->append(new MPCommandJob(this, MP_GET_LOGIN,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        if (data[2] == 0 && !login.isEmpty()) return false;

        QString l = data.mid(2, data[0]);
        if (!login.isEmpty() && l != login)
            return false;

        jobs->user_data = l;

        return true;
    }));

    jobs->append(new MPCommandJob(this, MP_GET_PASSWORD,
                                  [=](const QByteArray &data, bool &) -> bool
    {
        if (data[2] == 0) return false;
        return true;
    }));

    connect(jobs, &AsyncJobs::finished, [=](const QByteArray &data)
    {
        //data is last result
        //all jobs finished success

        qInfo() << "Password retreived ok";
        QString pass = data.mid(2, data[0]);

        cb(true, jobs->user_data.toString(), pass);
    });

    connect(jobs, &AsyncJobs::failed, [=](AsyncJob *failedJob)
    {
        Q_UNUSED(failedJob);
        qCritical() << "Failed getting password";
        cb(false, QString(), QString());
    });

    jobs->start();
}

void MPDevice::getRandomNumber(std::function<void(bool success, const QByteArray &nums)> cb)
{
    AsyncJobs *jobs = new AsyncJobs(this);

    jobs->append(new MPCommandJob(this, MP_GET_RANDOM_NUMBER, QByteArray()));

    connect(jobs, &AsyncJobs::finished, [=](const QByteArray &data)
    {
        //data is last result
        //all jobs finished success

        qInfo() << "Random numbers generated ok";
        cb(true, data);
    });

    connect(jobs, &AsyncJobs::failed, [=](AsyncJob *failedJob)
    {
        Q_UNUSED(failedJob);
        qCritical() << "Failed getting password";
        cb(false, QByteArray());
    });

    jobs->start();
}
