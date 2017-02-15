/***************************************************************************
                         vfile.cpp
                     -------------------
    copyright            : (C) 2000 by Rafi Yanai
    e-mail               : krusader@users.sourceforge.net
    web site             : http://krusader.sourceforge.net
 ---------------------------------------------------------------------------

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

#include "vfile.h"

// QtCore
#include <QDateTime>
#include <QMimeDatabase>
#include <QMimeType>

#include <KConfigCore/KDesktopFile>

#include "krpermhandler.h"
#include "krvfshandler.h"

bool vfile::vfile_userDefinedFolderIcons = true;

// TODO set default vfile_size to '-1' to distinguish between empty directories and directories with
// unknown size

vfile::vfile(const QString &name, const QUrl &url, bool isDir,
             KIO::filesize_t size, mode_t mode, time_t mtime,
             uid_t uid, gid_t gid, const QString &owner, const QString &group,
             bool isLink, const QString &linkDest, bool isBrokenLink,
             const QString &acl, const QString &defaultAcl)
    : m_name(name), m_url(url), m_isDir(isDir),
      m_size(size), m_mode(mode), m_mtime(mtime),
      m_uid(uid), m_gid(gid), m_owner(owner), m_group(group),
      m_isLink(isLink), m_linkDest(linkDest), m_isBrokenLink(isBrokenLink),
      m_acl(acl), m_defaulfAcl(defaultAcl), m_AclLoaded(false),
      m_mimeType(), m_icon()
{
    m_permissions = KRpermHandler::mode2QString(mode);

    if (m_owner.isEmpty())
        m_owner = KRpermHandler::uid2user(m_uid);

    if (m_group.isEmpty())
        m_group = KRpermHandler::gid2group(m_gid);

    if (m_isDir && !m_isLink)
        m_size = 0;
}

vfile *vfile::createDummy()
{
    vfile *file = new vfile("..", QUrl(), true,
                            0, 0, 0);
    file->vfile_setIcon("go-up");
    return file;
}

vfile *vfile::createVirtualDir(const QString &name, const QUrl &url)
{
    return new vfile(name, url, true,
                     0, 0700, time(0),
                     getuid(), getgid());
}

vfile *vfile::createCopy(const vfile &file, const QString &newName)
{
    return new vfile(newName, file.vfile_getUrl(), file.vfile_isDir(),
                     file.vfile_getSize(), file.vfile_getMode(), file.vfile_getTime_t(),
                     file.m_uid, file.m_gid, file.vfile_getOwner(), file.vfile_getGroup(),
                     file.vfile_isSymLink(), file.vfile_getSymDest(), file.vfile_isBrokenLink());
}

char vfile::vfile_isReadable() const
{
    if (m_uid != (uid_t)-1 && m_gid != (gid_t)-1)
        return KRpermHandler::readable(m_permissions, m_gid, m_uid);
    else
        return KRpermHandler::ftpReadable(m_owner, m_url.userName(), m_permissions);
}

char vfile::vfile_isWriteable() const
{
    if (m_uid != (uid_t)-1 && m_gid != (gid_t)-1)
        return KRpermHandler::writeable(m_permissions, m_gid, m_uid);
    else
        return KRpermHandler::ftpWriteable(m_owner, m_url.userName(), m_permissions);
}

char vfile::vfile_isExecutable() const
{
    if (m_uid != (uid_t)-1 && m_gid != (gid_t)-1)
        return KRpermHandler::executable(m_permissions, m_gid, m_uid);
    else
        return KRpermHandler::ftpExecutable(m_owner, m_url.userName(), m_permissions);
}

const QString &vfile::vfile_getMime()
{
    if (m_mimeType.isEmpty()) {
        if (m_isDir) {
            m_mimeType = "inode/directory";
            m_icon = "inode-directory";
        } else if (vfile_isBrokenLink()) {
            m_mimeType = "unknown";
            m_icon = "file-broken";
        } else {
            const QMimeDatabase db;
            const QMimeType mt = db.mimeTypeForUrl(vfile_getUrl());
            m_mimeType = mt.isValid() ? mt.name() : "unknown";
            m_icon = mt.isValid() ? mt.iconName() : "file-broken";

            if (m_mimeType == "inode/directory") {
                // TODO view update needed? and does this ever happen?
                m_isDir = true;
            }
        }

        if (m_isDir && vfile_userDefinedFolderIcons) {
            const QUrl url = vfile_getUrl();
            if (url.isLocalFile()) {
                const QString file = url.toLocalFile() + "/.directory";
                const KDesktopFile cfg(file);
                const QString icon = cfg.readIcon();
                if (!icon.isEmpty())
                    m_icon = icon.startsWith(QLatin1String("./")) ?
                                     // relative path
                                     url.toLocalFile() + '/' + icon :
                                     icon;
            }
        }
    }
    return m_mimeType;
}

const QString &vfile::vfile_getIcon()
{
    if (m_icon.isEmpty()) {
        vfile_getMime(); // sets the icon
    }
    return m_icon;
}

const QString &vfile::vfile_getACL()
{
    if (!m_AclLoaded)
        vfile_loadACL();
    return m_acl;
}

void vfile::vfile_loadACL()
{
    if (m_url.isLocalFile()) {
        KrVfsHandler::getACL(this, m_acl, m_defaulfAcl);
    }
    m_AclLoaded = true;
}

const KIO::UDSEntry vfile::vfile_getEntry()
{
    KIO::UDSEntry entry;

    entry.insert(KIO::UDSEntry::UDS_NAME, vfile_getName());
    entry.insert(KIO::UDSEntry::UDS_SIZE, vfile_getSize());
    entry.insert(KIO::UDSEntry::UDS_MODIFICATION_TIME, vfile_getTime_t());
    entry.insert(KIO::UDSEntry::UDS_USER, vfile_getOwner());
    entry.insert(KIO::UDSEntry::UDS_GROUP, vfile_getGroup());
    entry.insert(KIO::UDSEntry::UDS_MIME_TYPE, vfile_getMime());
    entry.insert(KIO::UDSEntry::UDS_FILE_TYPE, vfile_getMode() & S_IFMT);
    entry.insert(KIO::UDSEntry::UDS_ACCESS, vfile_getMode() & 07777);

    if (vfile_isSymLink())
        entry.insert(KIO::UDSEntry::UDS_LINK_DEST, vfile_getSymDest());

    if (!m_AclLoaded)
        vfile_loadACL();
    if (!m_acl.isNull() || !m_defaulfAcl.isNull()) {
        entry.insert(KIO::UDSEntry::UDS_EXTENDED_ACL, 1);

        if (!m_acl.isNull())
            entry.insert(KIO::UDSEntry::UDS_ACL_STRING, m_acl);

        if (!m_defaulfAcl.isNull())
            entry.insert(KIO::UDSEntry::UDS_DEFAULT_ACL_STRING, m_defaulfAcl);
    }

    return entry;
}
