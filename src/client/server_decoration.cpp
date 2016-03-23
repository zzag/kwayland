/****************************************************************************
Copyright 2015  Martin Gräßlin <mgraesslin@kde.org>

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) version 3, or any
later version accepted by the membership of KDE e.V. (or its
successor approved by the membership of KDE e.V.), which shall
act as a proxy defined in Section 6 of version 3 of the license.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library.  If not, see <http://www.gnu.org/licenses/>.
****************************************************************************/
#include "server_decoration.h"
#include "event_queue.h"
#include "logging_p.h"
#include "surface.h"
#include "wayland_pointer_p.h"

#include <wayland-server-decoration-client-protocol.h>

#include <QVector>

namespace KWayland
{
namespace Client
{

class ServerSideDecorationManager::Private
{
public:
    Private() = default;

    void setup(org_kde_kwin_server_decoration_manager *serversidedecorationmanager);

    WaylandPointer<org_kde_kwin_server_decoration_manager, org_kde_kwin_server_decoration_manager_destroy> serversidedecorationmanager;
    EventQueue *queue = nullptr;
    ServerSideDecoration::Mode defaultMode = ServerSideDecoration::Mode::None;
    QVector<ServerSideDecoration*> decorations;

private:
    static void defaultModeCallback(void *data, org_kde_kwin_server_decoration_manager *manager, uint32_t mode);
    static const struct org_kde_kwin_server_decoration_manager_listener s_listener;
};

class ServerSideDecoration::Private
{
public:
    Private(ServerSideDecoration *q);

    void setup(org_kde_kwin_server_decoration *serversidedecoration);

    WaylandPointer<org_kde_kwin_server_decoration, org_kde_kwin_server_decoration_release> serversidedecoration;
    Mode mode = Mode::None;
    Mode defaultMode = Mode::None;

private:
    static void modeCallback(void *data, org_kde_kwin_server_decoration *org_kde_kwin_server_decoration, uint32_t mode);
    static const struct org_kde_kwin_server_decoration_listener s_listener;

    ServerSideDecoration *q;
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const org_kde_kwin_server_decoration_manager_listener ServerSideDecorationManager::Private::s_listener = {
    defaultModeCallback
};
#endif

void ServerSideDecorationManager::Private::defaultModeCallback(void *data, org_kde_kwin_server_decoration_manager *manager, uint32_t mode)
{
    auto p = reinterpret_cast<ServerSideDecorationManager::Private*>(data);
    Q_ASSERT(p->serversidedecorationmanager == manager);

    ServerSideDecoration::Mode m;
    switch (mode) {
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_NONE:
        m = ServerSideDecoration::Mode::None;
        break;
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT:
        m = ServerSideDecoration::Mode::Client;
        break;
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER:
        m = ServerSideDecoration::Mode::Server;
        break;
    default:
        // invalid mode cannot set
        qCWarning(KWAYLAND_CLIENT) << "Invalid decoration mode pushed by Server:" << mode;
        return;
    }
    p->defaultMode = m;
    // update the default mode on all decorations
    for (auto it = p->decorations.constBegin(); it != p->decorations.constEnd(); ++it) {
        (*it)->d->defaultMode = m;
        // TODO: do we need a signal?
    }
}

void ServerSideDecorationManager::Private::setup(org_kde_kwin_server_decoration_manager *manager)
{
    Q_ASSERT(manager);
    Q_ASSERT(!serversidedecorationmanager);
    serversidedecorationmanager.setup(manager);
    org_kde_kwin_server_decoration_manager_add_listener(serversidedecorationmanager, &s_listener, this);
}

ServerSideDecorationManager::ServerSideDecorationManager(QObject *parent)
    : QObject(parent)
    , d(new Private)
{
}

ServerSideDecorationManager::~ServerSideDecorationManager()
{
    release();
}

void ServerSideDecorationManager::setup(org_kde_kwin_server_decoration_manager *serversidedecorationmanager)
{
    d->setup(serversidedecorationmanager);
}

void ServerSideDecorationManager::release()
{
    d->serversidedecorationmanager.release();
}

void ServerSideDecorationManager::destroy()
{
    d->serversidedecorationmanager.destroy();
}

void ServerSideDecorationManager::setEventQueue(EventQueue *queue)
{
    d->queue = queue;
}

EventQueue *ServerSideDecorationManager::eventQueue()
{
    return d->queue;
}

ServerSideDecorationManager::operator org_kde_kwin_server_decoration_manager*() {
    return d->serversidedecorationmanager;
}

ServerSideDecorationManager::operator org_kde_kwin_server_decoration_manager*() const {
    return d->serversidedecorationmanager;
}

bool ServerSideDecorationManager::isValid() const
{
    return d->serversidedecorationmanager.isValid();
}

ServerSideDecoration *ServerSideDecorationManager::create(Surface *surface, QObject *parent)
{
    return create(*surface, parent);
}

ServerSideDecoration *ServerSideDecorationManager::create(wl_surface *surface, QObject *parent)
{
    Q_ASSERT(isValid());
    ServerSideDecoration *deco = new ServerSideDecoration(parent);
    auto w = org_kde_kwin_server_decoration_manager_create(d->serversidedecorationmanager, surface);
    if (d->queue) {
        d->queue->addProxy(w);
    }
    deco->d->defaultMode = d->defaultMode;
    deco->d->mode = d->defaultMode;
    deco->setup(w);
    return deco;
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS
const org_kde_kwin_server_decoration_listener ServerSideDecoration::Private::s_listener = {
    modeCallback
};
#endif

ServerSideDecoration::Private::Private(ServerSideDecoration *q)
    : q(q)
{
}

void ServerSideDecoration::Private::modeCallback(void *data, org_kde_kwin_server_decoration *decoration, uint32_t mode)
{
    Q_UNUSED(decoration)
    Private *p = reinterpret_cast<Private*>(data);
    Mode m;
    switch (mode) {
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_NONE:
        m = Mode::None;
        break;
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT:
        m = Mode::Client;
        break;
    case ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER:
        m = Mode::Server;
        break;
    default:
        // invalid mode cannot set
        qCWarning(KWAYLAND_CLIENT) << "Invalid decoration mode pushed by Server:" << mode;
        return;
    }
    p->mode = m;
    emit p->q->modeChanged();
}

ServerSideDecoration::ServerSideDecoration(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
}

ServerSideDecoration::~ServerSideDecoration()
{
    release();
}

void ServerSideDecoration::Private::setup(org_kde_kwin_server_decoration *s)
{
    Q_ASSERT(s);
    Q_ASSERT(!serversidedecoration.isValid());
    serversidedecoration.setup(s);
    org_kde_kwin_server_decoration_add_listener(serversidedecoration, &s_listener, this);
}

void ServerSideDecoration::setup(org_kde_kwin_server_decoration *serversidedecoration)
{
    d->setup(serversidedecoration);
}

void ServerSideDecoration::release()
{
    d->serversidedecoration.release();
}

void ServerSideDecoration::destroy()
{
    d->serversidedecoration.destroy();
}

ServerSideDecoration::operator org_kde_kwin_server_decoration*() {
    return d->serversidedecoration;
}

ServerSideDecoration::operator org_kde_kwin_server_decoration*() const {
    return d->serversidedecoration;
}

bool ServerSideDecoration::isValid() const
{
    return d->serversidedecoration.isValid();
}

void ServerSideDecoration::requestMode(Mode mode)
{
    Q_ASSERT(d->serversidedecoration.isValid());
    uint32_t wlMode = 0;
    switch (mode) {
    case Mode::None:
        wlMode = ORG_KDE_KWIN_SERVER_DECORATION_MODE_NONE;
        break;
    case Mode::Client:
        wlMode = ORG_KDE_KWIN_SERVER_DECORATION_MODE_CLIENT;
        break;
    case Mode::Server:
        wlMode = ORG_KDE_KWIN_SERVER_DECORATION_MODE_SERVER;
        break;
    default:
        Q_UNREACHABLE();
    }
    org_kde_kwin_server_decoration_request_mode(d->serversidedecoration, wlMode);
}

ServerSideDecoration::Mode ServerSideDecoration::mode() const
{
    return d->mode;
}

ServerSideDecoration::Mode ServerSideDecoration::defaultMode() const
{
    return d->defaultMode;
}

}
}