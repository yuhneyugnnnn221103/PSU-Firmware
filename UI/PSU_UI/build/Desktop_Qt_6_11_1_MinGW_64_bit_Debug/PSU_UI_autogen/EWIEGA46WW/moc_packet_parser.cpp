/****************************************************************************
** Meta object code from reading C++ file 'packet_parser.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../packet_parser.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'packet_parser.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13Packet_ParserE_t {};
} // unnamed namespace

template <> constexpr inline auto Packet_Parser::qt_create_metaobjectdata<qt_meta_tag_ZN13Packet_ParserE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Packet_Parser",
        "packetReceived",
        "",
        "MeasurementPacket",
        "packet",
        "limitAckReceived",
        "LimitAck",
        "ack",
        "statusReceived",
        "StatusPacket",
        "status",
        "crcError",
        "frameError",
        "processData",
        "data",
        "reset"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'packetReceived'
        QtMocHelpers::SignalData<void(const MeasurementPacket &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Signal 'limitAckReceived'
        QtMocHelpers::SignalData<void(const LimitAck &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'statusReceived'
        QtMocHelpers::SignalData<void(const StatusPacket &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Signal 'crcError'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'frameError'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'processData'
        QtMocHelpers::SlotData<void(const QByteArray &)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QByteArray, 14 },
        }}),
        // Slot 'reset'
        QtMocHelpers::SlotData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Packet_Parser, qt_meta_tag_ZN13Packet_ParserE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Packet_Parser::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13Packet_ParserE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13Packet_ParserE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13Packet_ParserE_t>.metaTypes,
    nullptr
} };

void Packet_Parser::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Packet_Parser *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->packetReceived((*reinterpret_cast<std::add_pointer_t<MeasurementPacket>>(_a[1]))); break;
        case 1: _t->limitAckReceived((*reinterpret_cast<std::add_pointer_t<LimitAck>>(_a[1]))); break;
        case 2: _t->statusReceived((*reinterpret_cast<std::add_pointer_t<StatusPacket>>(_a[1]))); break;
        case 3: _t->crcError(); break;
        case 4: _t->frameError(); break;
        case 5: _t->processData((*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[1]))); break;
        case 6: _t->reset(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Packet_Parser::*)(const MeasurementPacket & )>(_a, &Packet_Parser::packetReceived, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Packet_Parser::*)(const LimitAck & )>(_a, &Packet_Parser::limitAckReceived, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Packet_Parser::*)(const StatusPacket & )>(_a, &Packet_Parser::statusReceived, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Packet_Parser::*)()>(_a, &Packet_Parser::crcError, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Packet_Parser::*)()>(_a, &Packet_Parser::frameError, 4))
            return;
    }
}

const QMetaObject *Packet_Parser::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Packet_Parser::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13Packet_ParserE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Packet_Parser::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 7)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 7;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 7)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 7;
    }
    return _id;
}

// SIGNAL 0
void Packet_Parser::packetReceived(const MeasurementPacket & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void Packet_Parser::limitAckReceived(const LimitAck & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void Packet_Parser::statusReceived(const StatusPacket & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void Packet_Parser::crcError()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void Packet_Parser::frameError()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
QT_WARNING_POP
