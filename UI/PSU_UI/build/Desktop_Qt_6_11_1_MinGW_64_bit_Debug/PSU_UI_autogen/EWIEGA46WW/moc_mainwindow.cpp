/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../mainwindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10MainWindowE_t {};
} // unnamed namespace

template <> constexpr inline auto MainWindow::qt_create_metaobjectdata<qt_meta_tag_ZN10MainWindowE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "MainWindow",
        "onConnectClicked",
        "",
        "onRefreshPortsClicked",
        "onLogClicked",
        "onClearPlotsClicked",
        "onWindowSecChanged",
        "index",
        "onPowerEnableClicked",
        "onPowerDisableClicked",
        "onPowerCommandSent",
        "enable",
        "onMeasurementUpdated",
        "MeasurementPacket",
        "packet",
        "onSystemStatusUpdated",
        "SystemStatus",
        "status",
        "onConnectionStateChanged",
        "connected",
        "onCommunicationStatsChanged",
        "onLinkStaleChanged",
        "stale",
        "onErrorOccurred",
        "message",
        "onLoggingStateChanged",
        "logging"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'onConnectClicked'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onRefreshPortsClicked'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLogClicked'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onClearPlotsClicked'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onWindowSecChanged'
        QtMocHelpers::SlotData<void(int)>(6, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Slot 'onPowerEnableClicked'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPowerDisableClicked'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onPowerCommandSent'
        QtMocHelpers::SlotData<void(bool)>(10, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 11 },
        }}),
        // Slot 'onMeasurementUpdated'
        QtMocHelpers::SlotData<void(const MeasurementPacket &)>(12, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Slot 'onSystemStatusUpdated'
        QtMocHelpers::SlotData<void(const SystemStatus &)>(15, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Slot 'onConnectionStateChanged'
        QtMocHelpers::SlotData<void(bool)>(18, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 19 },
        }}),
        // Slot 'onCommunicationStatsChanged'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'onLinkStaleChanged'
        QtMocHelpers::SlotData<void(bool)>(21, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 22 },
        }}),
        // Slot 'onErrorOccurred'
        QtMocHelpers::SlotData<void(const QString &)>(23, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 24 },
        }}),
        // Slot 'onLoggingStateChanged'
        QtMocHelpers::SlotData<void(bool)>(25, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 26 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<MainWindow, qt_meta_tag_ZN10MainWindowE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10MainWindowE_t>.metaTypes,
    nullptr
} };

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<MainWindow *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->onConnectClicked(); break;
        case 1: _t->onRefreshPortsClicked(); break;
        case 2: _t->onLogClicked(); break;
        case 3: _t->onClearPlotsClicked(); break;
        case 4: _t->onWindowSecChanged((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->onPowerEnableClicked(); break;
        case 6: _t->onPowerDisableClicked(); break;
        case 7: _t->onPowerCommandSent((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 8: _t->onMeasurementUpdated((*reinterpret_cast<std::add_pointer_t<MeasurementPacket>>(_a[1]))); break;
        case 9: _t->onSystemStatusUpdated((*reinterpret_cast<std::add_pointer_t<SystemStatus>>(_a[1]))); break;
        case 10: _t->onConnectionStateChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 11: _t->onCommunicationStatsChanged(); break;
        case 12: _t->onLinkStaleChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->onErrorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->onLoggingStateChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10MainWindowE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    return _id;
}
QT_WARNING_POP
