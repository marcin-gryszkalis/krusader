/***************************************************************************
                                krpermhandler.cpp
                            -------------------
   copyright            : (C) 2000 by Shie Erlich & Rafi Yanai
   email                : krusader@users.sourceforge.net
   web site   : http://krusader.sourceforge.net
---------------------------------------------------------------------------
 Description
***************************************************************************

 A

    db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
    88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
    88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
    88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
    88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
    YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                                    S o u r c e    F i l e

***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "krpermhandler.h"

// QtCore
#include <QDateTime>
#include <QDir>
#include <QLocale>
#include <qplatformdefs.h>

QHash<QString, uid_t> *KRpermHandler::passwdCache = 0L;
QHash<QString, gid_t> *KRpermHandler::groupCache = 0L;
QHash<int, char>    *KRpermHandler::currentGroups = 0L;
QHash<int, QString> *KRpermHandler::uidCache = 0L;
QHash<int, QString> *KRpermHandler::gidCache = 0L;

char KRpermHandler::writeable(QString perm, gid_t gid, uid_t uid, int rwx)
{
    if (rwx != -1)
        return (rwx & W_OK) ? ALLOWED_PERM : NO_PERM;
    // root override
    if (getuid() == 0)
        return ALLOWED_PERM;
    // first check other permissions.
    if (perm[ 8 ] != '-') return ALLOWED_PERM;
    // now check group permission
    if ((perm[ 5 ] != '-') && (currentGroups->find(gid) != currentGroups->end()))
        return ALLOWED_PERM;
    // the last chance - user permissions
    if ((perm[ 2 ] != '-') && (uid == getuid()))
        return ALLOWED_PERM;
    // sorry !
    return NO_PERM;
}

char KRpermHandler::readable(QString perm, gid_t gid, uid_t uid, int rwx)
{
    if (rwx != -1)
        return (rwx & R_OK) ? ALLOWED_PERM : NO_PERM;
    // root override
    if (getuid() == 0)
        return ALLOWED_PERM;
    // first check other permissions.
    if (perm[ 7 ] != '-') return ALLOWED_PERM;
    // now check group permission
    if ((perm[ 4 ] != '-') && (currentGroups->find(gid) != currentGroups->end()))
        return ALLOWED_PERM;
    // the last chance - user permissions
    if ((perm[ 1 ] != '-') && (uid == getuid()))
        return ALLOWED_PERM;
    // sorry !
    return NO_PERM;
}

char KRpermHandler::executable(QString perm, gid_t gid, uid_t uid, int rwx)
{
    if (rwx != -1)
        return (rwx & X_OK) ? ALLOWED_PERM : NO_PERM;
    // first check other permissions.
    if (perm[ 9 ] != '-') return ALLOWED_PERM;
    // now check group permission
    if ((perm[ 6 ] != '-') && (currentGroups->find(gid) != currentGroups->end()))
        return ALLOWED_PERM;
    // the last chance - user permissions
    if ((perm[ 3 ] != '-') && (uid == getuid()))
        return ALLOWED_PERM;
    // sorry !
    return NO_PERM;
}

bool KRpermHandler::fileWriteable(QString localFile)
{
    QT_STATBUF stat_p;
    if (QT_STAT(localFile.toLocal8Bit(), &stat_p) == -1) return false;
    mode_t m = stat_p.st_mode;
    QString perm = mode2QString(m);
    return writeable(perm, stat_p.st_gid, stat_p.st_uid);
}

bool KRpermHandler::fileReadable(QString localFile)
{
    QT_STATBUF stat_p;
    if (QT_STAT(localFile.toLocal8Bit(), &stat_p) == -1) return false;
    mode_t m = stat_p.st_mode;
    QString perm = mode2QString(m);
    return readable(perm, stat_p.st_gid, stat_p.st_uid);
}

bool KRpermHandler::fileExecutable(QString localFile)
{
    QT_STATBUF stat_p;
    if (QT_STAT(localFile.toLocal8Bit(), &stat_p) == -1) return false;
    mode_t m = stat_p.st_mode;
    QString perm = mode2QString(m);
    return executable(perm, stat_p.st_gid, stat_p.st_uid);
}

QString KRpermHandler::mode2QString(mode_t m)
{
    char perm[ 11 ];
    for (int i = 0; i != 10; i++)
        perm[ i ] = '-';
    perm[ 10 ] = 0;

    if (S_ISLNK(m)) perm[ 0 ] = 'l';      // check for symLink
    if (S_ISDIR(m)) perm[ 0 ] = 'd';      // check for directory

    //ReadUser = 0400, WriteUser = 0200, ExeUser = 0100, Suid = 04000
    if (m & 0400) perm[ 1 ] = 'r';
    if (m & 0200) perm[ 2 ] = 'w';
    if (m & 0100) perm[ 3 ] = 'x';
    if (m & 04000) perm[ 3 ] = 's';
    //ReadGroup = 0040, WriteGroup = 0020, ExeGroup = 0010, Gid = 02000
    if (m & 0040) perm[ 4 ] = 'r';
    if (m & 0020) perm[ 5 ] = 'w';
    if (m & 0010) perm[ 6 ] = 'x';
    if (m & 02000) perm[ 6 ] = 's';
    //ReadOther = 0004, WriteOther = 0002, ExeOther = 0001, Sticky = 01000
    if (m & 0004) perm[ 7 ] = 'r';
    if (m & 0002) perm[ 8 ] = 'w';
    if (m & 0001) perm[ 9 ] = 'x';
    if (m & 01000) perm[ 9 ] = 't';

    return QString(perm);
}

void KRpermHandler::init()
{
    // set the umask to 022
    //umask( 022 );

    // 50 groups should be enough
    gid_t groupList[ 50 ];
    int groupNo = getgroups(50, groupList);

    // init the groups and user caches
    passwdCache = new QHash<QString, uid_t>();
    groupCache = new QHash<QString, gid_t>();
    currentGroups = new QHash<int, char>();
    uidCache = new QHash<int, QString>();
    gidCache = new QHash<int, QString>();
// In kdewin32 implementation as of 4.1.2, getpwent always returns the same struct
#ifndef Q_WS_WIN
    // fill the UID cache
    struct passwd *pass;
    while ((pass = getpwent()) != 0L) {
        passwdCache->insert(pass->pw_name, pass->pw_uid);
        (*uidCache)[ pass->pw_uid ] = QString(pass->pw_name);
    }
    delete pass;
    endpwent();

    // fill the GID cache
    struct group *gr;
    while ((gr = getgrent()) != 0L) {
        groupCache->insert(gr->gr_name, gr->gr_gid);
        (*gidCache)[ gr->gr_gid ] = QString(gr->gr_name);
    }
    delete gr;
    endgrent();
#endif
    // fill the groups for the current user
    for (int i = 0; i < groupNo; ++i) {
        (*currentGroups)[ groupList[ i ] ] = char(1);
    }
    // just to be sure add the effective gid...
    (*currentGroups)[ getegid()] = char(1);
}

char KRpermHandler::ftpWriteable(QString fileOwner, QString userName, QString perm)
{
    // first check other permissions.
    if (perm[ 8 ] != '-') return ALLOWED_PERM;
    // can't check group permission !
    // so check the user permissions
    if ((perm[ 2 ] != '-') && (fileOwner == userName))
        return ALLOWED_PERM;
    if ((perm[ 2 ] != '-') && (userName.isEmpty()))
        return UNKNOWN_PERM;
    if (perm[ 5 ] != '-') return UNKNOWN_PERM;
    return NO_PERM;
}

char KRpermHandler::ftpReadable(QString fileOwner, QString userName, QString perm)
{
    // first check other permissions.
    if (perm[ 7 ] != '-') return ALLOWED_PERM;
    // can't check group permission !
    // so check the user permissions
    if ((perm[ 1 ] != '-') && (fileOwner == userName))
        return ALLOWED_PERM;
    if ((perm[ 1 ] != '-') && (userName.isEmpty()))
        return UNKNOWN_PERM;
    if (perm[ 4 ] != '-') return UNKNOWN_PERM;
    return NO_PERM;
}

char KRpermHandler::ftpExecutable(QString fileOwner, QString userName, QString perm)
{
    // first check other permissions.
    if (perm[ 9 ] != '-') return ALLOWED_PERM;
    // can't check group permission !
    // so check the user permissions
    if ((perm[ 3 ] != '-') && (fileOwner == userName))
        return ALLOWED_PERM;
    if ((perm[ 3 ] != '-') && (userName.isEmpty()))
        return UNKNOWN_PERM;
    if (perm[ 6 ] != '-') return UNKNOWN_PERM;
    return NO_PERM;
}

bool KRpermHandler::dirExist(QString path)
{
    QT_DIR * dir = QT_OPENDIR(path.toLocal8Bit());
    if (!dir) return false;
    QT_CLOSEDIR(dir);   // bug fix Karai Csaba (ckarai)
    return true;
}

bool KRpermHandler::fileExist(QString fullPath)
{
    if (fullPath.right(1) == "/") fullPath = fullPath.left(fullPath.length() - 1) ;
    if (fullPath.left(1) != "/") return fileExist("/", fullPath);
    return fileExist(fullPath.left(fullPath.lastIndexOf("/")) ,
                     fullPath.mid(fullPath.lastIndexOf("/") + 1));
}

bool KRpermHandler::fileExist(QString path, QString name)
{
    if (QDir(path).exists(name)) return true;
    QT_DIR* dir = QT_OPENDIR(path.toLocal8Bit());
    if (!dir) return false;
    QT_DIRENT* dirEnt;
    while ((dirEnt = QT_READDIR(dir))) {
        if (dirEnt->d_name == name) {
            QT_CLOSEDIR(dir);
            return true;
        }
    }
    QT_CLOSEDIR(dir);
    return false;
}

QString KRpermHandler::parseSize(KIO::filesize_t val)
{
    return QLocale().toString(val);
#if 0
    QString temp;
    temp.sprintf("%llu", val);
    if (temp.length() <= 3) return temp;
    unsigned int i = temp.length() % 3;
    if (i == 0) i = 3;
    QString size = temp.left(i) + ",";
    while (i + 3 < temp.length()) {
        size = size + temp.mid(i, 3) + ",";
        i += 3;
    }
    size = size + temp.right(3);

    return size;
#endif
}

QString KRpermHandler::date2qstring(QString date)
{
    QString temp;
    int year;

    year = date[ 6 ].digitValue() * 10 + date[ 7 ].digitValue();
    year > 80 ? year += 1900 : year += 2000;

    temp.sprintf("%d", year);
    temp = temp + date[ 3 ] + date[ 4 ] + date[ 0 ] + date[ 1 ] + date[ 9 ] + date[ 10 ] + date[ 12 ] + date[ 13 ];

    return temp;
}

time_t KRpermHandler::QString2time(QString date)
{
    struct tm t;
    t.tm_sec = 0;
    t.tm_min = (QString(date[ 12 ]) + QString(date[ 13 ])).toInt();
    t.tm_hour = (QString(date[ 9 ]) + QString(date[ 10 ])).toInt();
    t.tm_mday = (QString(date[ 0 ]) + QString(date[ 1 ])).toInt();
    t.tm_mon = (QString(date[ 3 ]) + QString(date[ 4 ])).toInt() - 1;
    t.tm_year = (QString(date[ 6 ]) + QString(date[ 7 ])).toInt();
    if (t.tm_year < 70) t.tm_year += 100;
    t.tm_isdst = -1; // daylight saving time information isn't available

    return mktime(&t);
}

gid_t KRpermHandler::group2gid(QString group)
{
    if (groupCache->find(group) == groupCache->end())
        return getgid();
    return (*groupCache)[ group ];
}
uid_t KRpermHandler::user2uid(QString user)
{
    if (passwdCache->find(user) == passwdCache->end())
        return getuid();
    return (*passwdCache)[ user ];
}

QString KRpermHandler::gid2group(gid_t groupId)
{
    if (gidCache->find(groupId) == gidCache->end())
        return QString("???");
    return (*gidCache)[ groupId ];
}

QString KRpermHandler::uid2user(uid_t userId)
{
    if (uidCache->find(userId) == uidCache->end())
        return QString("???");
    return (*uidCache)[ userId ];
}
