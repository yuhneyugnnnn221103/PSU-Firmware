/****************************************************************************
** Meta object code from reading C++ file 'ina228_widget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../ina228_widget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'ina228_widget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13Ina228_WidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto Ina228_Widget::qt_create_metaobjectdata<qt_meta_tag_ZN13Ina228_WidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Ina228_Widget",
        "setLimitsRequested",
        "",
        "channel",
        "sovlA",
        "bovlV",
        "buvlV",
        "getLimitsRequested",
        "setData",
        "Ina228Data",
        "data",
        "setFresh",
        "fresh",
        "setFault",
        "fault",
        "setStale",
        "stale",
        "applyAck",
        "LimitAck",
        "ack",
        "setLimitsEnabled",
        "enabled",
        "clearValues"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'setLimitsRequested'
        QtMocHelpers::SignalData<void(int, double, double, double)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { QMetaType::Double, 4 }, { QMetaType::Double, 5 }, { QMetaType::Double, 6 },
        }}),
        // Signal 'getLimitsRequested'
        QtMocHelpers::SignalData<void(int)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Slot 'setData'
        QtMocHelpers::SlotData<void(const Ina228Data &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 9, 10 },
        }}),
        // Slot 'setFresh'
        QtMocHelpers::SlotData<void(bool)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 },
        }}),
        // Slot 'setFault'
        QtMocHelpers::SlotData<void(bool)>(13, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 14 },
        }}),
        // Slot 'setStale'
        QtMocHelpers::SlotData<void(bool)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 16 },
        }}),
        // Slot 'applyAck'
        QtMocHelpers::SlotData<void(const LimitAck &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 18, 19 },
        }}),
        // Slot 'setLimitsEnabled'
        QtMocHelpers::SlotData<void(bool)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 21 },
        }}),
        // Slot 'clearValues'
        QtMocHelpers::SlotData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Ina228_Widget, qt_meta_tag_ZN13Ina228_WidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Ina228_Widget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13Ina228_WidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13Ina228_WidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13Ina228_WidgetE_t>.metaTypes,
    nullptr
} };

void Ina228_Widget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Ina228_Widget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->setLimitsRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<double>>(_a[4]))); break;
        case 1: _t->getLimitsRequested((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 2: _t->setData((*reinterpret_cast<std::add_pointer_t<Ina228Data>>(_a[1]))); break;
        case 3: _t->setFresh((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->setFault((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->setStale((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->applyAck((*reinterpret_cast<std::add_pointer_t<LimitAck>>(_a[1]))); break;
        case 7: _t->setLimitsEnabled((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->clearValues(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Ina228_Widget::*)(int , double , double , double )>(_a, &Ina228_Widget::setLimitsRequested, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Ina228_Widget::*)(int )>(_a, &Ina228_Widget::getLimitsRequested, 1))
            return;
    }
}

const QMetaObject *Ina228_Widget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Ina228_Widget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13Ina228_WidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int Ina228_Widget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void Ina228_Widget::setLimitsRequested(int _t1, double _t2, double _t3, double _t4)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2, _t3, _t4);
}

// SIGNAL 1
void Ina228_Widget::getLimitsRequested(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}
QT_WARNING_POP
