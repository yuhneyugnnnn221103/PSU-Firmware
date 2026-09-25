/****************************************************************************
** Meta object code from reading C++ file 'measurement_controller.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../measurement_controller.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'measurement_controller.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN22Measurement_ControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto Measurement_Controller::qt_create_metaobjectdata<qt_meta_tag_ZN22Measurement_ControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Measurement_Controller",
        "measurementUpdated",
        "",
        "MeasurementPacket",
        "packet",
        "systemStatusUpdated",
        "SystemStatus",
        "status",
        "connectionStateChanged",
        "connected",
        "communicationStatsChanged",
        "linkStaleChanged",
        "stale",
        "powerCommandSent",
        "enable",
        "limitAckReceived",
        "LimitAck",
        "ack",
        "safetyStatusUpdated",
        "StatusPacket",
        "errorOccurred",
        "message",
        "onPacketReceived",
        "onLimitAckReceived",
        "onStatusReceived",
        "onSerialConnected",
        "onSerialDisconnected",
        "onSerialError",
        "onCrcError",
        "onFrameError",
        "onWatchdogTick"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'measurementUpdated'
        QtMocHelpers::SignalData<void(const MeasurementPacket &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'systemStatusUpdated'
        QtMocHelpers::SignalData<void(const SystemStatus &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'connectionStateChanged'
        QtMocHelpers::SignalData<void(bool)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 9 },
        }}),
        // Signal 'communicationStatsChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'linkStaleChanged'
        QtMocHelpers::SignalData<void(bool)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 },
        }}),
        // Signal 'powerCommandSent'
        QtMocHelpers::SignalData<void(bool)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 14 },
        }}),
        // Signal 'limitAckReceived'
        QtMocHelpers::SignalData<void(const LimitAck &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Signal 'safetyStatusUpdated'
        QtMocHelpers::SignalData<void(const StatusPacket &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 19, 7 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Slot 'onPacketReceived'
        QtMocHelpers::SlotData<void(const MeasurementPacket &)>(22, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'onLimitAckReceived'
        QtMocHelpers::SlotData<void(const LimitAck &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Slot 'onStatusReceived'
        QtMocHelpers::SlotData<void(const StatusPacket &)>(24, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 19, 7 },
        }}),
        // Slot 'onSerialConnected'
        QtMocHelpers::SlotData<void()>(25, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSerialDisconnected'
        QtMocHelpers::SlotData<void()>(26, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onSerialError'
        QtMocHelpers::SlotData<void(const QString &)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Slot 'onCrcError'
        QtMocHelpers::SlotData<void()>(28, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onFrameError'
        QtMocHelpers::SlotData<void()>(29, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onWatchdogTick'
        QtMocHelpers::SlotData<void()>(30, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Measurement_Controller, qt_meta_tag_ZN22Measurement_ControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Measurement_Controller::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22Measurement_ControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22Measurement_ControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN22Measurement_ControllerE_t>.metaTypes,
    nullptr
} };

void Measurement_Controller::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Measurement_Controller *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->measurementUpdated((*reinterpret_cast<std::add_pointer_t<MeasurementPacket>>(_a[1]))); break;
        case 1: _t->systemStatusUpdated((*reinterpret_cast<std::add_pointer_t<SystemStatus>>(_a[1]))); break;
        case 2: _t->connectionStateChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->communicationStatsChanged(); break;
        case 4: _t->linkStaleChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->powerCommandSent((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->limitAckReceived((*reinterpret_cast<std::add_pointer_t<LimitAck>>(_a[1]))); break;
        case 7: _t->safetyStatusUpdated((*reinterpret_cast<std::add_pointer_t<StatusPacket>>(_a[1]))); break;
        case 8: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 9: _t->onPacketReceived((*reinterpret_cast<std::add_pointer_t<MeasurementPacket>>(_a[1]))); break;
        case 10: _t->onLimitAckReceived((*reinterpret_cast<std::add_pointer_t<LimitAck>>(_a[1]))); break;
        case 11: _t->onStatusReceived((*reinterpret_cast<std::add_pointer_t<StatusPacket>>(_a[1]))); break;
        case 12: _t->onSerialConnected(); break;
        case 13: _t->onSerialDisconnected(); break;
        case 14: _t->onSerialError((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 15: _t->onCrcError(); break;
        case 16: _t->onFrameError(); break;
        case 17: _t->onWatchdogTick(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(const MeasurementPacket & )>(_a, &Measurement_Controller::measurementUpdated, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(const SystemStatus & )>(_a, &Measurement_Controller::systemStatusUpdated, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(bool )>(_a, &Measurement_Controller::connectionStateChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)()>(_a, &Measurement_Controller::communicationStatsChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(bool )>(_a, &Measurement_Controller::linkStaleChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(bool )>(_a, &Measurement_Controller::powerCommandSent, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(const LimitAck & )>(_a, &Measurement_Controller::limitAckReceived, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(const StatusPacket & )>(_a, &Measurement_Controller::safetyStatusUpdated, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (Measurement_Controller::*)(const QString & )>(_a, &Measurement_Controller::errorOccurred, 8))
            return;
    }
}

const QMetaObject *Measurement_Controller::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Measurement_Controller::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN22Measurement_ControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Measurement_Controller::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 18;
    }
    return _id;
}

// SIGNAL 0
void Measurement_Controller::measurementUpdated(const MeasurementPacket & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void Measurement_Controller::systemStatusUpdated(const SystemStatus & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void Measurement_Controller::connectionStateChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void Measurement_Controller::communicationStatsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Measurement_Controller::linkStaleChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void Measurement_Controller::powerCommandSent(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void Measurement_Controller::limitAckReceived(const LimitAck & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void Measurement_Controller::safetyStatusUpdated(const StatusPacket & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void Measurement_Controller::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}
QT_WARNING_POP
