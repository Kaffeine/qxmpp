/*
 * Copyright (C) 2008-2019 The QXmpp developers
 *
 * Author:
 *  Manjeet Dahiya
 *
 * Source:
 *  https://github.com/qxmpp-project/qxmpp
 *
 * This file is a part of QXmpp library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 */


#include "QXmppPresence.h"
#include "QXmppUtils.h"
#include <QtDebug>
#include <QDateTime>
#include <QDomElement>
#include <QXmlStreamWriter>
#include "QXmppConstants_p.h"

static const char* presence_types[] = {
    "error",
    "",
    "unavailable",
    "subscribe",
    "subscribed",
    "unsubscribe",
    "unsubscribed",
    "probe"
};

static const char* presence_shows[] = {
    "",
    "away",
    "xa",
    "dnd",
    "chat",
    "invisible"
};

class QXmppPresencePrivate : public QSharedData
{
public:
    QXmppPresence::AvailableStatusType availableStatusType;
    int priority;
    QString statusText;
    QXmppPresence::Type type;

    /// XEP-0153: vCard-Based Avatars

    /// photoHash: the SHA1 hash of the avatar image data itself (not the base64-encoded version)
    /// in accordance with RFC 3174
    QByteArray photoHash;
    QXmppPresence::VCardUpdateType vCardUpdateType;

    // XEP-0115: Entity Capabilities
    QString capabilityHash;
    QString capabilityNode;
    QByteArray capabilityVer;
    // Legacy XEP-0115: Entity Capabilities
    QStringList capabilityExt;

    // XEP-0045: Multi-User Chat
    QXmppMucItem mucItem;
    QString mucPassword;
    QList<int> mucStatusCodes;
    bool mucSupported;

    // XEP-0319: Last User Interaction in Presence
    QDateTime lastUserInteraction;

    // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
    QString mixUserJid;
    QString mixUserNick;
};

/// Constructs a QXmppPresence.
///
/// \param type

QXmppPresence::QXmppPresence(QXmppPresence::Type type)
    : d(new QXmppPresencePrivate)
{
    d->availableStatusType = Online;
    d->priority = 0;
    d->type = type;
    d->mucSupported = false;
    d->vCardUpdateType = VCardUpdateNone;
}

/// Constructs a copy of \a other.

QXmppPresence::QXmppPresence(const QXmppPresence &other)
    : QXmppStanza(other)
    , d(other.d)
{
}

/// Destroys a QXmppPresence.

QXmppPresence::~QXmppPresence()
{

}

/// Assigns \a other to this presence.

QXmppPresence &QXmppPresence::operator=(const QXmppPresence &other)
{
    QXmppStanza::operator=(other);
    d = other.d;
    return *this;
}

/// Returns the availability status type, for instance busy or away.
///
/// This will not tell you whether a contact is connected, check whether
/// type() is QXmppPresence::Available instead.

QXmppPresence::AvailableStatusType QXmppPresence::availableStatusType() const
{
    return d->availableStatusType;
}

/// Sets the availability status type, for instance busy or away.

void QXmppPresence::setAvailableStatusType(AvailableStatusType type)
{
    d->availableStatusType = type;
}

/// Returns the priority level of the resource.

int QXmppPresence::priority() const
{
    return d->priority;
}

/// Sets the \a priority level of the resource.

void QXmppPresence::setPriority(int priority)
{
    d->priority = priority;
}

/// Returns the status text, a textual description of the user's status.

QString QXmppPresence::statusText() const
{
    return d->statusText;
}

/// Sets the status text, a textual description of the user's status.
///
/// \param statusText The status text, for example "Gone fishing".

void QXmppPresence::setStatusText(const QString& statusText)
{
    d->statusText = statusText;
}

/// Returns the presence type.
///
/// You can use this method to determine the action which needs to be
/// taken in response to receiving the presence. For instance, if the type is
/// QXmppPresence::Available or QXmppPresence::Unavailable, you could update
/// the icon representing a contact's availability.

QXmppPresence::Type QXmppPresence::type() const
{
    return d->type;
}

/// Sets the presence type.
///
/// \param type

void QXmppPresence::setType(QXmppPresence::Type type)
{
    d->type = type;
}

/// \cond
void QXmppPresence::parse(const QDomElement &element)
{
    QXmppStanza::parse(element);

    const QString type = element.attribute("type");
    for (int i = Error; i <= Probe; i++) {
        if (type == presence_types[i]) {
            d->type = static_cast<Type>(i);
            break;
        }
    }
    const QString show = element.firstChildElement("show").text();
    for (int i = Online; i <= Invisible; i++) {
        if (show == presence_shows[i]) {
            d->availableStatusType = static_cast<AvailableStatusType>(i);
            break;
        }
    }
    d->statusText = element.firstChildElement("status").text();
    d->priority = element.firstChildElement("priority").text().toInt();

    QXmppElementList extensions;
    QDomElement xElement = element.firstChildElement();
    d->vCardUpdateType = VCardUpdateNone;
    while(!xElement.isNull())
    {
        // XEP-0045: Multi-User Chat
        if(xElement.namespaceURI() == ns_muc) {
            d->mucSupported = true;
            d->mucPassword = xElement.firstChildElement("password").text();
        }
        else if(xElement.namespaceURI() == ns_muc_user)
        {
            QDomElement itemElement = xElement.firstChildElement("item");
            d->mucItem.parse(itemElement);
            QDomElement statusElement = xElement.firstChildElement("status");
            d->mucStatusCodes.clear();
            while (!statusElement.isNull()) {
                d->mucStatusCodes << statusElement.attribute("code").toInt();
                statusElement = statusElement.nextSiblingElement("status");
            }
        }
        // XEP-0153: vCard-Based Avatars
        else if(xElement.namespaceURI() == ns_vcard_update)
        {
            QDomElement photoElement = xElement.firstChildElement("photo");
            if(!photoElement.isNull())
            {
                d->photoHash = QByteArray::fromHex(photoElement.text().toLatin1());
                if(d->photoHash.isEmpty())
                    d->vCardUpdateType = VCardUpdateNoPhoto;
                else
                    d->vCardUpdateType = VCardUpdateValidPhoto;
            }
            else
            {
                d->photoHash = QByteArray();
                d->vCardUpdateType = VCardUpdateNotReady;
            }
        }
        // XEP-0115: Entity Capabilities
        else if(xElement.tagName() == "c" && xElement.namespaceURI() == ns_capabilities)
        {
            d->capabilityNode = xElement.attribute("node");
            d->capabilityVer = QByteArray::fromBase64(xElement.attribute("ver").toLatin1());
            d->capabilityHash = xElement.attribute("hash");
            d->capabilityExt = xElement.attribute("ext").split(" ", QString::SkipEmptyParts);
        }
        // XEP-0319: Last User Interaction in Presence
        else if (xElement.tagName() == "idle" && xElement.namespaceURI() == ns_idle)
        {
            if (xElement.hasAttribute("since")) {
                const QString since = xElement.attribute("since");
                d->lastUserInteraction = QXmppUtils::datetimeFromString(since);
            }
        }
        // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
        else if (xElement.tagName() == "mix" && xElement.namespaceURI() == ns_mix_presence) {
            d->mixUserJid = xElement.firstChildElement("jid").text();
            d->mixUserNick = xElement.firstChildElement("nick").text();
        }
        else if (xElement.tagName() != "addresses" && xElement.tagName() != "error"
                 && xElement.tagName() != "show" && xElement.tagName() != "status"
                 && xElement.tagName() != "priority")
        {
            // other extensions
            extensions << QXmppElement(xElement);
        }
        xElement = xElement.nextSiblingElement();
    }
    setExtensions(extensions);
}

void QXmppPresence::toXml(QXmlStreamWriter *xmlWriter) const
{
    xmlWriter->writeStartElement("presence");
    helperToXmlAddAttribute(xmlWriter,"xml:lang", lang());
    helperToXmlAddAttribute(xmlWriter,"id", id());
    helperToXmlAddAttribute(xmlWriter,"to", to());
    helperToXmlAddAttribute(xmlWriter,"from", from());
    helperToXmlAddAttribute(xmlWriter,"type", presence_types[d->type]);

    const QString show = presence_shows[d->availableStatusType];
    if (!show.isEmpty())
        helperToXmlAddTextElement(xmlWriter, "show", show);
    if (!d->statusText.isEmpty())
        helperToXmlAddTextElement(xmlWriter, "status", d->statusText);
    if (d->priority != 0)
        helperToXmlAddTextElement(xmlWriter, "priority", QString::number(d->priority));

    error().toXml(xmlWriter);

    // XEP-0045: Multi-User Chat
    if(d->mucSupported) {
        xmlWriter->writeStartElement("x");
        xmlWriter->writeAttribute("xmlns", ns_muc);
        if (!d->mucPassword.isEmpty())
            xmlWriter->writeTextElement("password", d->mucPassword);
        xmlWriter->writeEndElement();
    }

    if(!d->mucItem.isNull() || !d->mucStatusCodes.isEmpty())
    {
        xmlWriter->writeStartElement("x");
        xmlWriter->writeAttribute("xmlns", ns_muc_user);
        if (!d->mucItem.isNull())
            d->mucItem.toXml(xmlWriter);
        foreach (int code, d->mucStatusCodes) {
            xmlWriter->writeStartElement("status");
            xmlWriter->writeAttribute("code", QString::number(code));
            xmlWriter->writeEndElement();
        }
        xmlWriter->writeEndElement();
    }

    // XEP-0153: vCard-Based Avatars
    if(d->vCardUpdateType != VCardUpdateNone)
    {
        xmlWriter->writeStartElement("x");
        xmlWriter->writeAttribute("xmlns", ns_vcard_update);
        switch(d->vCardUpdateType)
        {
        case VCardUpdateNoPhoto:
            helperToXmlAddTextElement(xmlWriter, "photo", "");
            break;
        case VCardUpdateValidPhoto:
            helperToXmlAddTextElement(xmlWriter, "photo", d->photoHash.toHex());
            break;
        case VCardUpdateNotReady:
            break;
        default:
            break;
        }
        xmlWriter->writeEndElement();
    }

    if(!d->capabilityNode.isEmpty() && !d->capabilityVer.isEmpty()
        && !d->capabilityHash.isEmpty())
    {
        xmlWriter->writeStartElement("c");
        xmlWriter->writeAttribute("xmlns", ns_capabilities);
        helperToXmlAddAttribute(xmlWriter, "hash", d->capabilityHash);
        helperToXmlAddAttribute(xmlWriter, "node", d->capabilityNode);
        helperToXmlAddAttribute(xmlWriter, "ver", d->capabilityVer.toBase64());
        xmlWriter->writeEndElement();
    }

    // XEP-0319: Last User Interaction in Presence
    if (!d->lastUserInteraction.isNull() && d->lastUserInteraction.isValid())
    {
        xmlWriter->writeStartElement("idle");
        xmlWriter->writeAttribute("xmlns", ns_idle);
        helperToXmlAddAttribute(xmlWriter, "since", QXmppUtils::datetimeToString(
                                d->lastUserInteraction));
        xmlWriter->writeEndElement();
    }

    // XEP-0405: Mediated Information eXchange (MIX): Participant Server Requirements
    if (!d->mixUserJid.isEmpty() || !d->mixUserNick.isEmpty()) {
        xmlWriter->writeStartElement("mix");
        xmlWriter->writeAttribute("xmlns", ns_mix_presence);
        if (!d->mixUserJid.isEmpty())
            xmlWriter->writeTextElement("jid", d->mixUserJid);
        if (!d->mixUserNick.isEmpty())
            xmlWriter->writeTextElement("nick", d->mixUserNick);
        xmlWriter->writeEndElement();
    }

    // other extensions
    QXmppStanza::extensionsToXml(xmlWriter);

    xmlWriter->writeEndElement();
}
/// \endcond

/// Returns the photo-hash of the VCardUpdate.
///
/// \return QByteArray

QByteArray QXmppPresence::photoHash() const
{
    return d->photoHash;
}

/// Sets the photo-hash of the VCardUpdate.
///
/// \param photoHash as QByteArray

void QXmppPresence::setPhotoHash(const QByteArray& photoHash)
{
    d->photoHash = photoHash;
}

/// Returns the type of VCardUpdate
///
/// \return VCardUpdateType

QXmppPresence::VCardUpdateType QXmppPresence::vCardUpdateType() const
{
    return d->vCardUpdateType;
}

/// Sets the type of VCardUpdate
///
/// \param type VCardUpdateType

void QXmppPresence::setVCardUpdateType(VCardUpdateType type)
{
    d->vCardUpdateType = type;
}

/// XEP-0115: Entity Capabilities
QString QXmppPresence::capabilityHash() const
{
    return d->capabilityHash;
}

/// XEP-0115: Entity Capabilities
void QXmppPresence::setCapabilityHash(const QString& hash)
{
    d->capabilityHash = hash;
}

/// XEP-0115: Entity Capabilities
QString QXmppPresence::capabilityNode() const
{
    return d->capabilityNode;
}

/// XEP-0115: Entity Capabilities
void QXmppPresence::setCapabilityNode(const QString& node)
{
    d->capabilityNode = node;
}

/// XEP-0115: Entity Capabilities
QByteArray QXmppPresence::capabilityVer() const
{
    return d->capabilityVer;
}

/// XEP-0115: Entity Capabilities
void QXmppPresence::setCapabilityVer(const QByteArray& ver)
{
    d->capabilityVer = ver;
}

/// Legacy XEP-0115: Entity Capabilities
QStringList QXmppPresence::capabilityExt() const
{
    return d->capabilityExt;
}

/// Returns the MUC item.

QXmppMucItem QXmppPresence::mucItem() const
{
    return d->mucItem;
}

/// Sets the MUC item.
///
/// \param item

void QXmppPresence::setMucItem(const QXmppMucItem &item)
{
    d->mucItem = item;
}

/// Returns the password used to join a MUC room.

QString QXmppPresence::mucPassword() const
{
    return d->mucPassword;
}

/// Sets the password used to join a MUC room.

void QXmppPresence::setMucPassword(const QString &password)
{
    d->mucPassword = password;
}

/// Returns the MUC status codes.

QList<int> QXmppPresence::mucStatusCodes() const
{
    return d->mucStatusCodes;
}

/// Sets the MUC status codes.
///
/// \param codes

void QXmppPresence::setMucStatusCodes(const QList<int> &codes)
{
    d->mucStatusCodes = codes;
}

/// Returns true if the sender has indicated MUC support.

bool QXmppPresence::isMucSupported() const
{
    return d->mucSupported;
}

/// Sets whether MUC is \a supported.

void QXmppPresence::setMucSupported(bool supported)
{
    d->mucSupported = supported;
}

/// Returns when the last user interaction with the client took place. See
/// XEP-0319: Last User Interaction in Presence for details.

QDateTime QXmppPresence::lastUserInteraction() const
{
    return d->lastUserInteraction;
}

/// Sets the time of the last user interaction as defined in XEP-0319: Last
/// User Interaction in Presence.

void QXmppPresence::setLastUserInteraction(const QDateTime& lastUserInteraction)
{
    d->lastUserInteraction = lastUserInteraction;
}

/// Returns the actual (full) JID of the MIX channel participant.

QString QXmppPresence::mixUserJid() const
{
    return d->mixUserJid;
}

/// Sets the actual (full) JID of the MIX channel participant.

void QXmppPresence::setMixUserJid(const QString& mixUserJid)
{
    d->mixUserJid = mixUserJid;
}

/// Returns the MIX participant's nickname.

QString QXmppPresence::mixUserNick() const
{
    return d->mixUserNick;
}

/// Sets the MIX participant's nickname.

void QXmppPresence::setMixUserNick(const QString& mixUserNick)
{
    d->mixUserNick = mixUserNick;
}

/// Indicates if the QXmppStanza is a stanza in the XMPP sence (i. e. a message,
/// iq or presence)

bool QXmppPresence::isXmppStanza() const
{
    return true;
}
