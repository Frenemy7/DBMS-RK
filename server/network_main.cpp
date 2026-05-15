#include "include/network/TcpServer.h"
#include "include/network/NetworkProtocol.h"

#include "src/storage/StorageEngineImpl.h"
#include "src/catalog/CatalogManagerImpl.h"
#include "src/integrity/IntegrityManagerImpl.h"
#include "src/index/IndexManagerImpl.h"
#include "src/maintenance/DatabaseMaintenanceImpl.h"
#include "src/security/SecurityManagerImpl.h"

#include <QCoreApplication>
#include <QDebug>

#include <windows.h>

int main(int argc, char *argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    QCoreApplication app(argc, argv);

    Storage::StorageEngineImpl storageEngine;
    Catalog::CatalogManagerImpl catalogManager(&storageEngine);
    Integrity::IntegrityManagerImpl integrityManager(&catalogManager, &storageEngine);
    Index::IndexManagerImpl indexManager(&storageEngine);
    Maintenance::DatabaseMaintenanceImpl maintenance;
    Security::SecurityManagerImpl security;

    if (!catalogManager.initSystem()) {
        qCritical() << "Failed to initialize DBMS metadata.";
        return 1;
    }

    Network::SqlService sqlService(
        &catalogManager,
        &storageEngine,
        &integrityManager,
        &maintenance,
        &security,
        &indexManager);

    Network::TcpServer server(&sqlService);
    const quint16 port = Network::kDefaultPort;
    if (!server.listen(QHostAddress::Any, port)) {
        qCritical() << "Failed to listen on port" << port << server.errorString();
        return 2;
    }

    qInfo() << "RuankoDBMS server listening on port" << port;
    return app.exec();
}
