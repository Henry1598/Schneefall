/****************************************************************************
** Meta object code from reading C++ file 'app.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../src/app.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'app.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
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
struct qt_meta_tag_ZN3AppE_t {};
} // unnamed namespace

template <> constexpr inline auto App::qt_create_metaobjectdata<qt_meta_tag_ZN3AppE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "App",
        "D-Bus Interface",
        "org.schneefall",
        "Fenster",
        "",
        "json",
        "Steuerfenster",
        "Beenden",
        "Umschalten",
        "Abschuetteln",
        "Status",
        "Fensterliste"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'Fenster'
        QtMocHelpers::SlotData<void(const QString &)>(3, 4, QMC::AccessPublic | QMC::MethodScriptable, QMetaType::Void, {{
            { QMetaType::QString, 5 },
        }}),
        // Slot 'Steuerfenster'
        QtMocHelpers::SlotData<void()>(6, 4, QMC::AccessPublic | QMC::MethodScriptable, QMetaType::Void),
        // Slot 'Beenden'
        QtMocHelpers::SlotData<void()>(7, 4, QMC::AccessPublic | QMC::MethodScriptable, QMetaType::Void),
        // Slot 'Umschalten'
        QtMocHelpers::SlotData<void()>(8, 4, QMC::AccessPublic | QMC::MethodScriptable, QMetaType::Void),
        // Slot 'Abschuetteln'
        QtMocHelpers::SlotData<void()>(9, 4, QMC::AccessPublic | QMC::MethodScriptable, QMetaType::Void),
        // Slot 'Status'
        QtMocHelpers::SlotData<QString()>(10, 4, QMC::AccessPublic | QMC::MethodScriptable, QMetaType::QString),
        // Slot 'Fensterliste'
        QtMocHelpers::SlotData<QString()>(11, 4, QMC::AccessPublic | QMC::MethodScriptable, QMetaType::QString),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    QtMocHelpers::UintData qt_constructors {};
    QtMocHelpers::ClassInfos qt_classinfo({
            {    1,    2 },
    });
    return QtMocHelpers::metaObjectData<App, qt_meta_tag_ZN3AppE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums, qt_constructors, qt_classinfo);
}
Q_CONSTINIT const QMetaObject App::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3AppE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3AppE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN3AppE_t>.metaTypes,
    nullptr
} };

void App::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<App *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->Fenster((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->Steuerfenster(); break;
        case 2: _t->Beenden(); break;
        case 3: _t->Umschalten(); break;
        case 4: _t->Abschuetteln(); break;
        case 5: { QString _r = _t->Status();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        case 6: { QString _r = _t->Fensterliste();
            if (_a[0]) *reinterpret_cast<QString*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
}

const QMetaObject *App::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *App::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN3AppE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int App::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
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
QT_WARNING_POP
