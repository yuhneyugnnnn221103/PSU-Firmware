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
        "setData",
        "",
        "Ina228Data",
        "data",
        "setFresh",
        "fresh",
        "setFault",
        "fault",
        "setStale",
        "stale",
        "clearValues"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'setData'
        QtMocHelpers::SlotData<void(const Ina228Data &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 3, 4 },
        }}),
        // Slot 'setFresh'
        QtMocHelpers::SlotData<void(bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 },
        }}),
        // Slot 'setFault'
        QtMocHelpers::SlotData<void(bool)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 },
        }}),
        // Slot 'setStale'
        QtMocHelpers::SlotData<void(bool)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 10 },
        }}),
        // Slot 'clearValues'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
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
        case 0: _t->setData((*reinterpret_cast<std::add_pointer_t<Ina228Data>>(_a[1]))); break;
        case 1: _t->setFresh((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 2: _t->setFault((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 3: _t->setStale((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->clearValues(); break;
        default: ;
        }
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
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}
QT_WARNING_POP
