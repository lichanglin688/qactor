#ifndef QACTOR_GLOBAL_H
#define QACTOR_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(QACTOR_BUILD)
#define QACTOR_EXPORT Q_DECL_EXPORT
#else
#define QACTOR_EXPORT Q_DECL_IMPORT
#endif

#endif
