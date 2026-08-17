#include <QtTest/QTest>
#include <type_traits>
#include <QTemporaryDir>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ArchitectureSnapshotAssembler.h"
#include "architecture/HybridRoutingPolicy.h"
#include "architecture/DatabaseResourceCatalog.h"
#include "architecture/DatabaseServiceBindingCatalog.h"
#include "architecture/DatabaseServiceDiscoveryProvider.h"
#include "architecture/DatabaseServiceSelectionCatalog.h"
#include "architecture/InMemoryResourceCatalog.h"
#include "architecture/InMemoryServiceBindingCatalog.h"
#include "architecture/InMemoryServiceSelectionCatalog.h"
#include "architecture/IResourceCatalog.h"
#include "architecture/IServiceBindingCatalog.h"
#include "architecture/IServiceDiscoveryProvider.h"
#include "architecture/IServiceSelectionCatalog.h"
#include "architecture/PersistedRuntimeArchitectureLoader.h"
#include "architecture/RuntimeArchitecturePresentation.h"
#include "architecture/RuntimeArchitectureFacade.h"
#include "architecture/RuntimeArchitectureQueryService.h"
#include "architecture/RuntimeArchitectureSnapshot.h"
#include "architecture/ResourceReference.h"
#include "architecture/ServiceBinding.h"
#include "architecture/ServiceCapability.h"
#include "architecture/ServiceCapabilityManifest.h"
#include "architecture/ServiceDiscoveryResult.h"
#include "architecture/ServiceDiscoverySnapshot.h"
#include "architecture/ServiceEndpoint.h"
#include "architecture/ServiceRegistryEntry.h"
#include "architecture/ServiceSelectionSnapshot.h"
#include "architecture/StaticServiceDiscoveryProvider.h"
#include "architecture/WorkspaceServiceBindingSnapshot.h"
#include "storage/DatabaseManager.h"
#include "storage/ServiceBindingRepository.h"
#include "storage/ServiceRegistryRepository.h"
#include "storage/ServiceResourceRepository.h"

class TestArchitectureReadiness : public QObject {
    Q_OBJECT

private slots:
    void default_values_are_empty_and_safe()
    {
        ServiceCapability capability;
        QVERIFY(capability.capabilityId.isEmpty());
        QVERIFY(capability.capabilityName.isEmpty());
        QVERIFY(capability.version.isEmpty());
        QCOMPARE(capability.enabled, false);

        ServiceBinding binding;
        QVERIFY(binding.boundServiceId.isEmpty());
        QCOMPARE(binding.sharedFilesEnabled, false);
        QCOMPARE(binding.sharedEditingEnabled, false);
        QCOMPARE(binding.connectorsEnabled, false);

        ServiceDiscoverySnapshot snapshot;
        QVERIFY(snapshot.serviceId.isEmpty());
        QVERIFY(snapshot.serviceName.isEmpty());
        QVERIFY(snapshot.organizationName.isEmpty());
        QVERIFY(snapshot.environmentName.isEmpty());
        QCOMPARE(snapshot.observedAtMs, 0);
        QCOMPARE(snapshot.capabilities.size(), 0);

        ResourceReference reference;
        QVERIFY(reference.serviceId.isEmpty());
        QVERIFY(reference.workspaceId.isEmpty());
        QVERIFY(reference.resourceId.isEmpty());
        QVERIFY(reference.resourceKind.isEmpty());
        QVERIFY(reference.title.isEmpty());
        QVERIFY(reference.version.isEmpty());
        QVERIFY(reference.summary.isEmpty());
        QCOMPARE(reference.origin, ResourceOrigin::Local);

        ServiceCapabilityManifest manifest;
        QVERIFY(manifest.serviceId.isEmpty());
        QVERIFY(manifest.serviceName.isEmpty());
        QVERIFY(manifest.version.isEmpty());
        QCOMPARE(manifest.capabilities.size(), 0);
    }

    void snapshot_can_hold_capabilities()
    {
        ServiceDiscoverySnapshot snapshot;
        snapshot.serviceId = QStringLiteral("svc-001");
        snapshot.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.organizationName = QStringLiteral("Demo Org");
        snapshot.environmentName = QStringLiteral("lan");
        snapshot.observedAtMs = 1712400000000LL;
        snapshot.capabilities.push_back(ServiceCapability{
            QStringLiteral("shared_files"),
            QStringLiteral("Shared Files"),
            QStringLiteral("1.0"),
            true
        });

        QCOMPARE(snapshot.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(snapshot.capabilities.size(), 1);
        QCOMPARE(snapshot.capabilities.front().capabilityId, QStringLiteral("shared_files"));
        QCOMPARE(snapshot.capabilities.front().enabled, true);

        ResourceReference reference;
        reference.serviceId = QStringLiteral("svc-001");
        reference.workspaceId = QStringLiteral("ws-001");
        reference.resourceId = QStringLiteral("file-001");
        reference.resourceKind = QStringLiteral("shared_file");
        reference.title = QStringLiteral("Design Spec");
        reference.version = QStringLiteral("v1");
        reference.summary = QStringLiteral("Shared file reference");
        reference.origin = ResourceOrigin::Service;

        QCOMPARE(reference.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(reference.origin, ResourceOrigin::Service);

        ServiceCapabilityManifest manifest;
        manifest.serviceId = QStringLiteral("svc-001");
        manifest.serviceName = QStringLiteral("LeyoChat Service");
        manifest.version = QStringLiteral("1.0");
        manifest.capabilities = snapshot.capabilities;

        QCOMPARE(manifest.capabilities.size(), 1);
        QCOMPARE(manifest.capabilities.front().capabilityName, QStringLiteral("Shared Files"));
    }

    void discovery_result_and_endpoint_are_safe_and_transport_ready()
    {
        ServiceEndpoint endpoint;
        QVERIFY(endpoint.serviceId.isEmpty());
        QVERIFY(endpoint.host.isEmpty());
        QCOMPARE(endpoint.port, static_cast<quint16>(0));
        QCOMPARE(endpoint.tlsEnabled, false);
        QVERIFY(endpoint.routePrefix.isEmpty());

        endpoint.serviceId = QStringLiteral("svc-001");
        endpoint.host = QStringLiteral("192.0.2.10");
        endpoint.port = static_cast<quint16>(8443);
        endpoint.tlsEnabled = true;
        endpoint.routePrefix = QStringLiteral("/api/v1");

        QCOMPARE(endpoint.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(endpoint.host, QStringLiteral("192.0.2.10"));
        QCOMPARE(endpoint.port, static_cast<quint16>(8443));
        QCOMPARE(endpoint.tlsEnabled, true);
        QCOMPARE(endpoint.routePrefix, QStringLiteral("/api/v1"));

        ServiceDiscoveryResult result;
        QVERIFY(result.services.isEmpty());
        QVERIFY(result.defaultServiceId.isEmpty());
        QCOMPARE(result.multipleServicesDetected, false);

        result.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("Demo Org"),
            QStringLiteral("lan"),
            1712400000000LL,
            {}
        });
        result.defaultServiceId = QStringLiteral("svc-001");
        result.multipleServicesDetected = false;

        QCOMPARE(result.services.size(), 1);
        QCOMPARE(result.services.front().serviceId, QStringLiteral("svc-001"));
        QCOMPARE(result.defaultServiceId, QStringLiteral("svc-001"));
    }

    void interface_layer_remains_abstract()
    {
        static_assert(std::is_abstract_v<IServiceDiscoveryProvider>);
        static_assert(std::is_abstract_v<IResourceCatalog>);
        static_assert(std::is_abstract_v<IServiceBindingCatalog>);
        static_assert(std::is_abstract_v<IServiceSelectionCatalog>);
        static_assert(std::is_destructible_v<IServiceDiscoveryProvider>);
        static_assert(std::is_destructible_v<IResourceCatalog>);
        static_assert(std::is_destructible_v<IServiceBindingCatalog>);
        static_assert(std::is_destructible_v<IServiceSelectionCatalog>);

        QVERIFY(true);
    }

    void binding_snapshots_form_a_small_closed_loop()
    {
        ServiceRegistryEntry registry;
        QVERIFY(registry.serviceId.isEmpty());
        QVERIFY(registry.serviceName.isEmpty());
        QVERIFY(registry.organizationName.isEmpty());
        QVERIFY(registry.environmentName.isEmpty());
        QVERIFY(registry.host.isEmpty());
        QCOMPARE(registry.port, static_cast<quint16>(0));
        QCOMPARE(registry.tlsEnabled, false);
        QCOMPARE(registry.capabilities.size(), 0);

        registry.serviceId = QStringLiteral("svc-001");
        registry.serviceName = QStringLiteral("LeyoChat Service");
        registry.organizationName = QStringLiteral("Demo Org");
        registry.environmentName = QStringLiteral("lan");
        registry.host = QStringLiteral("192.0.2.10");
        registry.port = static_cast<quint16>(8443);
        registry.tlsEnabled = true;
        registry.capabilities.push_back(ServiceCapability{
            QStringLiteral("shared_files"),
            QStringLiteral("Shared Files"),
            QStringLiteral("1.0"),
            true
        });

        GroupServiceBindingSnapshot groupBinding;
        QVERIFY(groupBinding.groupId.isEmpty());
        QVERIFY(groupBinding.groupName.isEmpty());
        QCOMPARE(groupBinding.enabled, false);
        QCOMPARE(groupBinding.binding.boundServiceId.isEmpty(), true);
        QCOMPARE(groupBinding.binding.sharedFilesEnabled, false);

        groupBinding.groupId = QStringLiteral("group-001");
        groupBinding.groupName = QStringLiteral("Design Group");
        groupBinding.enabled = true;
        groupBinding.binding.boundServiceId = registry.serviceId;
        groupBinding.binding.sharedFilesEnabled = true;
        groupBinding.registryEntry = registry;
        groupBinding.discoverySnapshot = ServiceDiscoverySnapshot{
            registry.serviceId,
            registry.serviceName,
            registry.organizationName,
            registry.environmentName,
            1712400000000LL,
            registry.capabilities
        };
        groupBinding.primaryResource = ResourceReference{
            registry.serviceId,
            QStringLiteral("ws-001"),
            QStringLiteral("resource-001"),
            QStringLiteral("shared_group"),
            QStringLiteral("Design Group"),
            QStringLiteral("v1"),
            QStringLiteral("Group service binding reference"),
            ResourceOrigin::Service
        };

        QCOMPARE(groupBinding.registryEntry.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(groupBinding.discoverySnapshot.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(groupBinding.primaryResource.resourceKind, QStringLiteral("shared_group"));
        QCOMPARE(groupBinding.binding.sharedFilesEnabled, true);

        WorkspaceServiceBindingSnapshot workspaceBinding;
        QVERIFY(workspaceBinding.workspaceId.isEmpty());
        QVERIFY(workspaceBinding.workspaceName.isEmpty());
        QCOMPARE(workspaceBinding.groupBindings.size(), 0);

        workspaceBinding.workspaceId = QStringLiteral("ws-001");
        workspaceBinding.workspaceName = QStringLiteral("Workspace One");
        workspaceBinding.groupBindings.push_back(groupBinding);

        QCOMPARE(workspaceBinding.groupBindings.size(), 1);
        QCOMPARE(workspaceBinding.groupBindings.front().groupId, QStringLiteral("group-001"));
        QCOMPARE(workspaceBinding.groupBindings.front().registryEntry.capabilities.size(), 1);
    }

    void selection_snapshot_closes_the_readiness_loop()
    {
        ServiceRegistryEntry registry;
        registry.serviceId = QStringLiteral("svc-001");
        registry.serviceName = QStringLiteral("LeyoChat Service");
        registry.organizationName = QStringLiteral("Demo Org");
        registry.environmentName = QStringLiteral("lan");
        registry.host = QStringLiteral("192.0.2.10");
        registry.port = static_cast<quint16>(8443);
        registry.tlsEnabled = true;
        registry.capabilities.push_back(ServiceCapability{
            QStringLiteral("shared_files"),
            QStringLiteral("Shared Files"),
            QStringLiteral("1.0"),
            true
        });

        GroupServiceBindingSnapshot groupBinding;
        groupBinding.groupId = QStringLiteral("group-001");
        groupBinding.groupName = QStringLiteral("Design Group");
        groupBinding.enabled = true;
        groupBinding.binding.boundServiceId = registry.serviceId;
        groupBinding.binding.sharedFilesEnabled = true;
        groupBinding.registryEntry = registry;
        groupBinding.discoverySnapshot = ServiceDiscoverySnapshot{
            registry.serviceId,
            registry.serviceName,
            registry.organizationName,
            registry.environmentName,
            1712400000000LL,
            registry.capabilities
        };
        groupBinding.primaryResource = ResourceReference{
            registry.serviceId,
            QStringLiteral("ws-001"),
            QStringLiteral("resource-001"),
            QStringLiteral("shared_group"),
            QStringLiteral("Design Group"),
            QStringLiteral("v1"),
            QStringLiteral("Group service binding reference"),
            ResourceOrigin::Service
        };

        ServiceSelectionSnapshot selection;
        QVERIFY(selection.workspaceId.isEmpty());
        QVERIFY(selection.groupId.isEmpty());
        QVERIFY(selection.serviceId.isEmpty());
        QVERIFY(selection.serviceName.isEmpty());
        QVERIFY(selection.selectionSource.isEmpty());
        QCOMPARE(selection.bound, false);

        selection.workspaceId = QStringLiteral("ws-001");
        selection.groupId = QStringLiteral("group-001");
        selection.serviceId = registry.serviceId;
        selection.serviceName = registry.serviceName;
        selection.selectionSource = QStringLiteral("group-binding");
        selection.registryEntry = registry;
        selection.discoverySnapshot = groupBinding.discoverySnapshot;
        selection.groupBinding = groupBinding;
        selection.selectedResource = groupBinding.primaryResource;
        selection.bound = true;

        QCOMPARE(selection.registryEntry.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(selection.discoverySnapshot.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(selection.groupBinding.groupId, QStringLiteral("group-001"));
        QCOMPARE(selection.selectedResource.workspaceId, QStringLiteral("ws-001"));
        QCOMPARE(selection.bound, true);
    }

    void runtime_architecture_presentation_formats_empty_snapshot()
    {
        const RuntimeArchitecturePresentation presentation =
            buildRuntimeArchitecturePresentation(0, 0, 0, 0, false, QString());

        QCOMPARE(presentation.chromeStatus, QStringLiteral("未发现服务"));
        QCOMPARE(presentation.welcomeSummary, QStringLiteral("当前未发现混合架构服务"));
        QVERIFY(presentation.welcomeDetail.contains(QStringLiteral("P2P")));
        QCOMPARE(presentation.panelSummary, QStringLiteral("当前没有已持久化的服务快照"));
    }

    void runtime_architecture_presentation_formats_bound_snapshot()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("Demo Org"),
            QStringLiteral("lan"),
            1712400000000LL,
            {}
        });
        snapshot.discoveryResult.defaultServiceId = QStringLiteral("svc-001");
        snapshot.workspaceBindings.push_back(WorkspaceServiceBindingSnapshot{
            QStringLiteral("ws-001"),
            QStringLiteral("设计工作区"),
            {}
        });
        GroupServiceBindingSnapshot groupBinding;
        groupBinding.groupId = QStringLiteral("group-001");
        groupBinding.groupName = QStringLiteral("设计群");
        groupBinding.enabled = true;
        groupBinding.binding.boundServiceId = QStringLiteral("svc-001");
        snapshot.groupBindings.push_back(groupBinding);
        snapshot.visibleResources.push_back(ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("file-001"),
            QStringLiteral("shared_file"),
            QStringLiteral("方案文档"),
            QStringLiteral("v1"),
            QStringLiteral("共享文档"),
            ResourceOrigin::Service
        });
        snapshot.selection.serviceId = QStringLiteral("svc-001");
        snapshot.selection.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selection.bound = true;

        const RuntimeArchitecturePresentation presentation =
            buildRuntimeArchitecturePresentation(snapshot);

        QCOMPARE(presentation.chromeStatus, QStringLiteral("1 个服务"));
        QCOMPARE(presentation.welcomeSummary, QStringLiteral("已发现 1 个服务，当前已绑定"));
        QVERIFY(presentation.welcomeDetail.contains(QStringLiteral("当前服务 LeyoChat Service")));
        QCOMPARE(presentation.panelSummary, QStringLiteral("运行时已装载 1 个服务快照"));
        QVERIFY(presentation.panelFootnote.contains(QStringLiteral("已绑定")));
    }

    void runtime_query_service_exposes_bound_group_state()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.groupBindings.push_back(GroupServiceBindingSnapshot{
            QStringLiteral("group-001"),
            QStringLiteral("设计群"),
            ServiceBinding{QStringLiteral("svc-001"), true, false, false},
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                8443,
                true,
                {}
            },
            ServiceDiscoverySnapshot{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                1712400000000LL,
                {}
            },
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_group"),
                QStringLiteral("Design Group"),
                QStringLiteral("v1"),
                QStringLiteral("Group binding reference"),
                ResourceOrigin::Service
            },
            true
        });
        snapshot.selection.groupId = QStringLiteral("group-001");
        snapshot.selection.workspaceId = QStringLiteral("ws-001");
        snapshot.selection.serviceId = QStringLiteral("svc-001");
        snapshot.selection.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selection.selectedResource = ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("file-001"),
            QStringLiteral("shared_file"),
            QStringLiteral("方案文档"),
            QStringLiteral("v3"),
            QStringLiteral("Design spec"),
            ResourceOrigin::Service
        };
        snapshot.selection.bound = true;

        RuntimeArchitectureQueryService query(snapshot);

        QVERIFY(query.hasBoundServiceForGroup(QStringLiteral("group-001")));
        QVERIFY(query.sharedFilesEnabledForGroup(QStringLiteral("group-001")));
        QCOMPARE(query.boundServiceIdForGroup(QStringLiteral("group-001")), QStringLiteral("svc-001"));
        QCOMPARE(query.primaryResourceForGroup(QStringLiteral("group-001")).title, QStringLiteral("Design Group"));
        QCOMPARE(query.selectedResourceForGroup(QStringLiteral("group-001")).title, QStringLiteral("方案文档"));
        QCOMPARE(query.serviceNameForGroup(QStringLiteral("group-001")), QStringLiteral("LeyoChat Service"));
    }

    void runtime_query_service_filters_shared_files_for_group()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.groupBindings.push_back(GroupServiceBindingSnapshot{
            QStringLiteral("group-001"),
            QStringLiteral("设计群"),
            ServiceBinding{QStringLiteral("svc-001"), true, false, false},
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                8443,
                true,
                {}
            },
            ServiceDiscoverySnapshot{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                1712400000000LL,
                {}
            },
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_group"),
                QStringLiteral("Design Group"),
                QStringLiteral("v1"),
                QStringLiteral("Group binding reference"),
                ResourceOrigin::Service
            },
            true
        });
        snapshot.visibleResources = {
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("file-001"),
                QStringLiteral("shared_file"),
                QStringLiteral("方案文档"),
                QStringLiteral("v3"),
                QStringLiteral("Spec"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("file-002"),
                QStringLiteral("group_file"),
                QStringLiteral("运行手册"),
                QStringLiteral("v1"),
                QStringLiteral("Runbook"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QStringLiteral("svc-002"),
                QStringLiteral("ws-002"),
                QStringLiteral("file-999"),
                QStringLiteral("shared_file"),
                QStringLiteral("Foreign File"),
                QStringLiteral("v9"),
                QStringLiteral("Other workspace"),
                ResourceOrigin::Service
            }
        };

        RuntimeArchitectureQueryService query(snapshot);
        const QVector<ResourceReference> visibleResources =
            query.visibleResourcesForGroup(QStringLiteral("group-001"));
        const QVector<ResourceReference> sharedFiles =
            query.sharedFileResourcesForGroup(QStringLiteral("group-001"));

        QCOMPARE(visibleResources.size(), 2);
        QCOMPARE(sharedFiles.size(), 2);
        QCOMPARE(sharedFiles.front().title, QStringLiteral("方案文档"));
        QCOMPARE(sharedFiles.back().title, QStringLiteral("运行手册"));
    }

    void hybrid_routing_policy_prefers_service_for_bound_group_files()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.groupBindings.push_back(GroupServiceBindingSnapshot{
            QStringLiteral("group-001"),
            QStringLiteral("设计群"),
            ServiceBinding{QStringLiteral("svc-001"), true, false, false},
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                8443,
                true,
                {}
            },
            ServiceDiscoverySnapshot{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                1712400000000LL,
                {}
            },
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_group"),
                QStringLiteral("Design Group"),
                QStringLiteral("v1"),
                QStringLiteral("Group binding reference"),
                ResourceOrigin::Service
            },
            true
        });
        snapshot.selection.groupId = QStringLiteral("group-001");
        snapshot.selection.workspaceId = QStringLiteral("ws-001");
        snapshot.selection.serviceId = QStringLiteral("svc-001");
        snapshot.selection.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selection.selectedResource = ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("file-001"),
            QStringLiteral("shared_file"),
            QStringLiteral("方案文档"),
            QStringLiteral("v3"),
            QStringLiteral("Design spec"),
            ResourceOrigin::Service
        };
        snapshot.selection.bound = true;

        const HybridRoutingDecision groupDecision =
            HybridRoutingPolicy::decideGroupFileRouting(snapshot, QStringLiteral("group-001"));
        QCOMPARE(groupDecision.mode, HybridRouteMode::ServicePreferred);
        QVERIFY(groupDecision.hasBoundService);
        QVERIFY(groupDecision.sharedFilesEnabled);
        QCOMPARE(groupDecision.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(groupDecision.selectedResourceTitle, QStringLiteral("方案文档"));

        const HybridRoutingDecision unknownDecision =
            HybridRoutingPolicy::decideGroupFileRouting(snapshot, QStringLiteral("group-404"));
        QCOMPARE(unknownDecision.mode, HybridRouteMode::P2POnly);
        QVERIFY(!unknownDecision.hasBoundService);
        QVERIFY(!unknownDecision.sharedFilesEnabled);
    }

    void static_discovery_provider_defaults_to_empty_result()
    {
        StaticServiceDiscoveryProvider provider;

        const ServiceDiscoveryResult result = provider.discoverServices();

        QVERIFY(result.services.isEmpty());
        QVERIFY(result.defaultServiceId.isEmpty());
        QCOMPARE(result.multipleServicesDetected, false);
    }

    void static_discovery_provider_returns_snapshot_copy()
    {
        StaticServiceDiscoveryProvider provider;
        ServiceDiscoveryResult seeded;
        seeded.defaultServiceId = QStringLiteral("svc-001");
        seeded.multipleServicesDetected = true;
        seeded.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("Demo Org"),
            QStringLiteral("lan"),
            1712400000000LL,
            {
                ServiceCapability{
                    QStringLiteral("shared_files"),
                    QStringLiteral("Shared Files"),
                    QStringLiteral("1.0"),
                    true
                }
            }
        });

        provider.setDiscoveryResult(seeded);
        seeded.defaultServiceId = QStringLiteral("mutated");
        seeded.services.front().serviceName = QStringLiteral("Mutated");

        const ServiceDiscoveryResult result = provider.discoverServices();

        QCOMPARE(result.defaultServiceId, QStringLiteral("svc-001"));
        QCOMPARE(result.services.size(), 1);
        QCOMPARE(result.services.front().serviceName, QStringLiteral("LeyoChat Service"));
    }

    void in_memory_resource_catalog_returns_seeded_resources()
    {
        InMemoryResourceCatalog catalog;
        QVector<ResourceReference> resources{
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_file"),
                QStringLiteral("Design Spec"),
                QStringLiteral("v1"),
                QStringLiteral("Shared spec"),
                ResourceOrigin::Service
            }
        };

        catalog.setResources(resources);
        resources.front().title = QStringLiteral("Mutated");

        const QVector<ResourceReference> listed = catalog.listResources();

        QCOMPARE(listed.size(), 1);
        QCOMPARE(listed.front().title, QStringLiteral("Design Spec"));
        QCOMPARE(listed.front().origin, ResourceOrigin::Service);
    }

    void in_memory_binding_catalog_tracks_registry_and_bindings()
    {
        InMemoryServiceBindingCatalog catalog;
        QVector<ServiceRegistryEntry> registry{
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                static_cast<quint16>(8443),
                true,
                {
                    ServiceCapability{
                        QStringLiteral("shared_files"),
                        QStringLiteral("Shared Files"),
                        QStringLiteral("1.0"),
                        true
                    }
                }
            }
        };
        QVector<GroupServiceBindingSnapshot> groupBindings{
            GroupServiceBindingSnapshot{
                QStringLiteral("group-001"),
                QStringLiteral("Design Group"),
                ServiceBinding{QStringLiteral("svc-001"), true, false, false},
                registry.front(),
                ServiceDiscoverySnapshot{
                    QStringLiteral("svc-001"),
                    QStringLiteral("LeyoChat Service"),
                    QStringLiteral("Demo Org"),
                    QStringLiteral("lan"),
                    1712400000000LL,
                    registry.front().capabilities
                },
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("group-res"),
                    QStringLiteral("shared_group"),
                    QStringLiteral("Design Group"),
                    QStringLiteral("v1"),
                    QStringLiteral("Binding resource"),
                    ResourceOrigin::Service
                },
                true
            }
        };
        QVector<WorkspaceServiceBindingSnapshot> workspaceBindings{
            WorkspaceServiceBindingSnapshot{
                QStringLiteral("ws-001"),
                QStringLiteral("Workspace One"),
                groupBindings
            }
        };

        catalog.setServiceRegistry(registry);
        catalog.setGroupBindings(groupBindings);
        catalog.setWorkspaceBindings(workspaceBindings);

        registry.front().serviceName = QStringLiteral("Mutated");
        groupBindings.front().groupName = QStringLiteral("Mutated Group");
        workspaceBindings.front().workspaceName = QStringLiteral("Mutated Workspace");

        QCOMPARE(catalog.listServiceRegistry().front().serviceName, QStringLiteral("LeyoChat Service"));
        QCOMPARE(catalog.listGroupBindings().front().groupName, QStringLiteral("Design Group"));
        QCOMPARE(catalog.listWorkspaceBindings().front().workspaceName, QStringLiteral("Workspace One"));
    }

    void in_memory_selection_catalog_tracks_current_selection()
    {
        InMemoryServiceSelectionCatalog catalog;
        ServiceSelectionSnapshot snapshot;
        snapshot.workspaceId = QStringLiteral("ws-001");
        snapshot.groupId = QStringLiteral("group-001");
        snapshot.serviceId = QStringLiteral("svc-001");
        snapshot.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selectionSource = QStringLiteral("group-binding");
        snapshot.bound = true;

        catalog.setCurrentSelection(snapshot);
        snapshot.serviceName = QStringLiteral("Mutated");

        const ServiceSelectionSnapshot current = catalog.currentSelection();

        QCOMPARE(current.workspaceId, QStringLiteral("ws-001"));
        QCOMPARE(current.serviceName, QStringLiteral("LeyoChat Service"));
        QCOMPARE(current.bound, true);
    }

    void assembler_normalizes_discovery_result_and_prefers_requested_default()
    {
        QVector<ServiceDiscoverySnapshot> services{
            ServiceDiscoverySnapshot{
                QStringLiteral("svc-001"),
                QStringLiteral("Primary Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                1712400000000LL,
                {}
            },
            ServiceDiscoverySnapshot{
                QStringLiteral("svc-002"),
                QStringLiteral("Secondary Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                1712400001000LL,
                {}
            }
        };

        const ServiceDiscoveryResult result =
            assembleDiscoveryResult(services, QStringLiteral("svc-002"));

        QCOMPARE(result.services.size(), 2);
        QCOMPARE(result.defaultServiceId, QStringLiteral("svc-002"));
        QCOMPARE(result.multipleServicesDetected, true);
    }

    void assembler_can_roundtrip_registry_and_snapshot()
    {
        ServiceRegistryEntry registry;
        registry.serviceId = QStringLiteral("svc-001");
        registry.serviceName = QStringLiteral("LeyoChat Service");
        registry.organizationName = QStringLiteral("Demo Org");
        registry.environmentName = QStringLiteral("lan");
        registry.host = QStringLiteral("192.0.2.10");
        registry.port = static_cast<quint16>(9443);
        registry.tlsEnabled = true;
        registry.capabilities.push_back(ServiceCapability{
            QStringLiteral("shared_files"),
            QStringLiteral("Shared Files"),
            QStringLiteral("1.0"),
            true
        });

        const ServiceDiscoverySnapshot snapshot = assembleDiscoverySnapshot(registry, 1712400000000LL);
        const ServiceEndpoint endpoint = assembleServiceEndpoint(registry, QStringLiteral("/service"));
        const ServiceRegistryEntry reconstructed = assembleRegistryEntry(snapshot, endpoint);

        QCOMPARE(snapshot.serviceId, registry.serviceId);
        QCOMPARE(snapshot.capabilities.size(), 1);
        QCOMPARE(endpoint.routePrefix, QStringLiteral("/service"));
        QCOMPARE(reconstructed.host, QStringLiteral("192.0.2.10"));
        QCOMPARE(reconstructed.port, static_cast<quint16>(9443));
        QCOMPARE(reconstructed.capabilities.size(), 1);
    }

    void assembler_builds_group_and_workspace_binding_snapshots()
    {
        ServiceRegistryEntry registry;
        registry.serviceId = QStringLiteral("svc-001");
        registry.serviceName = QStringLiteral("LeyoChat Service");
        registry.organizationName = QStringLiteral("Demo Org");
        registry.environmentName = QStringLiteral("lan");
        registry.host = QStringLiteral("192.0.2.10");
        registry.port = static_cast<quint16>(8443);

        ServiceBinding binding;
        binding.boundServiceId = registry.serviceId;
        binding.sharedFilesEnabled = true;

        ResourceReference resource;
        resource.serviceId = registry.serviceId;
        resource.workspaceId = QStringLiteral("ws-001");
        resource.resourceId = QStringLiteral("group-res");
        resource.resourceKind = QStringLiteral("shared_group");
        resource.title = QStringLiteral("Design Group");
        resource.origin = ResourceOrigin::Service;

        const GroupServiceBindingSnapshot groupBinding = assembleGroupBindingSnapshot(
            QStringLiteral("group-001"),
            QStringLiteral("Design Group"),
            binding,
            registry,
            resource,
            true,
            1712400000000LL);
        const WorkspaceServiceBindingSnapshot workspaceBinding = assembleWorkspaceBindingSnapshot(
            QStringLiteral("ws-001"),
            QStringLiteral("Workspace One"),
            {groupBinding});

        QCOMPARE(groupBinding.groupId, QStringLiteral("group-001"));
        QCOMPARE(groupBinding.discoverySnapshot.serviceId, registry.serviceId);
        QCOMPARE(groupBinding.primaryResource.resourceKind, QStringLiteral("shared_group"));
        QCOMPARE(workspaceBinding.groupBindings.size(), 1);
        QCOMPARE(workspaceBinding.groupBindings.front().groupName, QStringLiteral("Design Group"));
    }

    void assembler_resolves_selection_from_workspace_binding()
    {
        ServiceRegistryEntry registry;
        registry.serviceId = QStringLiteral("svc-001");
        registry.serviceName = QStringLiteral("LeyoChat Service");
        registry.organizationName = QStringLiteral("Demo Org");
        registry.environmentName = QStringLiteral("lan");

        ServiceBinding binding;
        binding.boundServiceId = registry.serviceId;
        binding.sharedFilesEnabled = true;

        ResourceReference resource;
        resource.serviceId = registry.serviceId;
        resource.workspaceId = QStringLiteral("ws-001");
        resource.resourceId = QStringLiteral("group-res");
        resource.resourceKind = QStringLiteral("shared_group");
        resource.title = QStringLiteral("Design Group");
        resource.origin = ResourceOrigin::Service;

        const GroupServiceBindingSnapshot groupBinding = assembleGroupBindingSnapshot(
            QStringLiteral("group-001"),
            QStringLiteral("Design Group"),
            binding,
            registry,
            resource,
            true,
            1712400000000LL);
        const WorkspaceServiceBindingSnapshot workspaceBinding = assembleWorkspaceBindingSnapshot(
            QStringLiteral("ws-001"),
            QStringLiteral("Workspace One"),
            {groupBinding});

        const ServiceSelectionSnapshot selection = resolveServiceSelectionSnapshot(
            workspaceBinding,
            QStringLiteral("group-001"),
            QStringLiteral("group-binding"));

        QCOMPARE(selection.workspaceId, QStringLiteral("ws-001"));
        QCOMPARE(selection.groupId, QStringLiteral("group-001"));
        QCOMPARE(selection.serviceId, QStringLiteral("svc-001"));
        QCOMPARE(selection.selectionSource, QStringLiteral("group-binding"));
        QCOMPARE(selection.bound, true);
    }

    void runtime_facade_combines_discovery_binding_and_selection()
    {
        StaticServiceDiscoveryProvider discoveryProvider;
        discoveryProvider.setDiscoveryResult(ServiceDiscoveryResult{
            {
                ServiceDiscoverySnapshot{
                    QStringLiteral("svc-001"),
                    QStringLiteral("LeyoChat Service"),
                    QStringLiteral("Demo Org"),
                    QStringLiteral("lan"),
                    1712400000000LL,
                    {}
                }
            },
            QStringLiteral("svc-001"),
            false
        });

        InMemoryServiceBindingCatalog bindingCatalog;
        bindingCatalog.setServiceRegistry({
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                static_cast<quint16>(8443),
                true,
                {}
            }
        });
        bindingCatalog.setGroupBindings({
            GroupServiceBindingSnapshot{
                QStringLiteral("group-001"),
                QStringLiteral("Design Group"),
                ServiceBinding{QStringLiteral("svc-001"), true, false, false},
                ServiceRegistryEntry{
                    QStringLiteral("svc-001"),
                    QStringLiteral("LeyoChat Service"),
                    QStringLiteral("Demo Org"),
                    QStringLiteral("lan"),
                    QStringLiteral("192.0.2.10"),
                    static_cast<quint16>(8443),
                    true,
                    {}
                },
                ServiceDiscoverySnapshot{
                    QStringLiteral("svc-001"),
                    QStringLiteral("LeyoChat Service"),
                    QStringLiteral("Demo Org"),
                    QStringLiteral("lan"),
                    1712400000000LL,
                    {}
                },
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("group-res"),
                    QStringLiteral("shared_group"),
                    QStringLiteral("Design Group"),
                    QStringLiteral("v1"),
                    QStringLiteral("Binding resource"),
                    ResourceOrigin::Service
                },
                true
            }
        });
        bindingCatalog.setWorkspaceBindings({
            WorkspaceServiceBindingSnapshot{
                QStringLiteral("ws-001"),
                QStringLiteral("Workspace One"),
                bindingCatalog.listGroupBindings()
            }
        });

        InMemoryResourceCatalog resourceCatalog({
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_file"),
                QStringLiteral("Design Spec"),
                QStringLiteral("v1"),
                QStringLiteral("Service resource"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QStringLiteral("svc-002"),
                QStringLiteral("ws-002"),
                QStringLiteral("res-002"),
                QStringLiteral("shared_file"),
                QStringLiteral("Other Spec"),
                QStringLiteral("v2"),
                QStringLiteral("Foreign resource"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QStringLiteral(),
                QStringLiteral(),
                QStringLiteral("local-001"),
                QStringLiteral("draft"),
                QStringLiteral("Local Draft"),
                QStringLiteral("v1"),
                QStringLiteral("Local resource"),
                ResourceOrigin::Local
            }
        });

        InMemoryServiceSelectionCatalog selectionCatalog(ServiceSelectionSnapshot{
            QStringLiteral("ws-001"),
            QStringLiteral("group-001"),
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("group-binding"),
            {},
            {},
            {},
            {},
            true
        });

        const RuntimeArchitectureFacade facade(
            discoveryProvider, bindingCatalog, resourceCatalog, selectionCatalog);
        const RuntimeArchitectureSnapshot snapshot = facade.loadSnapshot();

        QCOMPARE(snapshot.discoveryResult.defaultServiceId, QStringLiteral("svc-001"));
        QCOMPARE(snapshot.serviceRegistry.size(), 1);
        QCOMPARE(snapshot.workspaceBindings.size(), 1);
        QCOMPARE(snapshot.groupBindings.size(), 1);
        QCOMPARE(snapshot.selection.groupId, QStringLiteral("group-001"));
        QCOMPARE(snapshot.visibleResources.size(), 2);
        QCOMPARE(snapshot.visibleResources.front().serviceId, QStringLiteral("svc-001"));
        QCOMPARE(snapshot.visibleResources.back().origin, ResourceOrigin::Local);
    }

    void runtime_facade_returns_all_resources_when_no_bound_service()
    {
        StaticServiceDiscoveryProvider discoveryProvider;
        InMemoryServiceBindingCatalog bindingCatalog;
        InMemoryResourceCatalog resourceCatalog({
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_file"),
                QStringLiteral("Design Spec"),
                QStringLiteral("v1"),
                QStringLiteral("Service resource"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QStringLiteral(),
                QStringLiteral(),
                QStringLiteral("local-001"),
                QStringLiteral("draft"),
                QStringLiteral("Local Draft"),
                QStringLiteral("v1"),
                QStringLiteral("Local resource"),
                ResourceOrigin::Local
            }
        });
        InMemoryServiceSelectionCatalog selectionCatalog;

        const RuntimeArchitectureFacade facade(
            discoveryProvider, bindingCatalog, resourceCatalog, selectionCatalog);
        const RuntimeArchitectureSnapshot snapshot = facade.loadSnapshot();

        QCOMPARE(snapshot.selection.bound, false);
        QCOMPARE(snapshot.visibleResources.size(), 2);
    }

    void runtime_facade_can_load_snapshot_from_persisted_state()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("architecture-runtime.db"));
        const QString connectionName = QStringLiteral("architecture-runtime-db");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ServiceRegistryRepository registryRepository(connectionName);
        ServiceBindingRepository bindingRepository(connectionName);
        ServiceResourceRepository resourceRepository(connectionName);

        QVERIFY(registryRepository.replaceRegistry({
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                static_cast<quint16>(8443),
                true,
                {
                    ServiceCapability{
                        QStringLiteral("shared_files"),
                        QStringLiteral("Shared Files"),
                        QStringLiteral("1.0"),
                        true
                    }
                }
            }
        }, QStringLiteral("svc-001"), 1712500000000LL));

        const QVector<GroupServiceBindingSnapshot> groupBindings{
            GroupServiceBindingSnapshot{
                QStringLiteral("group-001"),
                QStringLiteral("Design Group"),
                ServiceBinding{QStringLiteral("svc-001"), true, false, false},
                {},
                ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712500000000LL, {}},
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("group-res"),
                    QStringLiteral("shared_group"),
                    QStringLiteral("Design Group"),
                    QStringLiteral("v1"),
                    QStringLiteral("Binding resource"),
                    ResourceOrigin::Service
                },
                true
            }
        };
        QVERIFY(bindingRepository.replaceWorkspaceBindings({
            WorkspaceServiceBindingSnapshot{
                QStringLiteral("ws-001"),
                QStringLiteral("Workspace One"),
                groupBindings
            }
        }));
        QVERIFY(bindingRepository.replaceGroupBindings(groupBindings));
        QVERIFY(bindingRepository.saveCurrentSelection(ServiceSelectionSnapshot{
            QStringLiteral("ws-001"),
            QStringLiteral("group-001"),
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("group-binding"),
            {},
            ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712500000000LL, {}},
            {},
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral(),
                QString(),
                QString(),
                QString(),
                ResourceOrigin::Service
            },
            true
        }));
        QVERIFY(resourceRepository.replaceResources({
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_file"),
                QStringLiteral("Design Spec"),
                QStringLiteral("v1"),
                QStringLiteral("Service resource"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QString(),
                QString(),
                QStringLiteral("local-001"),
                QStringLiteral("draft"),
                QStringLiteral("Local Draft"),
                QStringLiteral("v1"),
                QStringLiteral("Local resource"),
                ResourceOrigin::Local
            }
        }));

        DatabaseServiceDiscoveryProvider discoveryProvider(registryRepository);
        DatabaseServiceBindingCatalog bindingCatalog(
            bindingRepository, registryRepository, resourceRepository);
        DatabaseResourceCatalog resourceCatalog(resourceRepository);
        DatabaseServiceSelectionCatalog selectionCatalog(
            bindingRepository, registryRepository, resourceRepository);

        const RuntimeArchitectureFacade facade(
            discoveryProvider, bindingCatalog, resourceCatalog, selectionCatalog);
        const RuntimeArchitectureSnapshot snapshot = facade.loadSnapshot();

        QCOMPARE(snapshot.discoveryResult.defaultServiceId, QStringLiteral("svc-001"));
        QCOMPARE(snapshot.serviceRegistry.size(), 1);
        QCOMPARE(snapshot.workspaceBindings.size(), 1);
        QCOMPARE(snapshot.workspaceBindings.front().groupBindings.size(), 1);
        QCOMPARE(snapshot.groupBindings.size(), 1);
        QCOMPARE(snapshot.groupBindings.front().registryEntry.serviceName, QStringLiteral("LeyoChat Service"));
        QCOMPARE(snapshot.selection.groupId, QStringLiteral("group-001"));
        QCOMPARE(snapshot.selection.selectedResource.resourceId, QStringLiteral("res-001"));
        QCOMPARE(snapshot.visibleResources.size(), 2);
        QCOMPARE(snapshot.visibleResources.front().origin, ResourceOrigin::Local);
        QCOMPARE(snapshot.visibleResources.back().serviceId, QStringLiteral("svc-001"));
    }

    void persisted_runtime_loader_returns_empty_snapshot_for_fresh_database()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("architecture-loader-empty.db"));
        const QString connectionName = QStringLiteral("architecture-loader-empty");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        PersistedRuntimeArchitectureLoader loader(connectionName);
        const RuntimeArchitectureSnapshot snapshot = loader.loadSnapshot();

        QVERIFY(snapshot.discoveryResult.services.isEmpty());
        QVERIFY(snapshot.serviceRegistry.isEmpty());
        QVERIFY(snapshot.workspaceBindings.isEmpty());
        QVERIFY(snapshot.groupBindings.isEmpty());
        QVERIFY(snapshot.visibleResources.isEmpty());
        QCOMPARE(snapshot.selection.bound, false);
    }

    void persisted_runtime_loader_can_load_snapshot_from_database()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString databasePath = tempDir.filePath(QStringLiteral("architecture-loader.db"));
        const QString connectionName = QStringLiteral("architecture-loader");

        DatabaseManager manager(databasePath, connectionName);
        QVERIFY(manager.open());

        ServiceRegistryRepository registryRepository(connectionName);
        ServiceBindingRepository bindingRepository(connectionName);
        ServiceResourceRepository resourceRepository(connectionName);

        QVERIFY(registryRepository.replaceRegistry({
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                static_cast<quint16>(8443),
                true,
                {
                    ServiceCapability{
                        QStringLiteral("shared_files"),
                        QStringLiteral("Shared Files"),
                        QStringLiteral("1.0"),
                        true
                    }
                }
            }
        }, QStringLiteral("svc-001"), 1712501000000LL));

        const QVector<GroupServiceBindingSnapshot> groupBindings{
            GroupServiceBindingSnapshot{
                QStringLiteral("group-001"),
                QStringLiteral("Design Group"),
                ServiceBinding{QStringLiteral("svc-001"), true, false, false},
                {},
                ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712501000000LL, {}},
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("group-res"),
                    QStringLiteral("shared_group"),
                    QStringLiteral("Design Group"),
                    QStringLiteral("v1"),
                    QStringLiteral("Binding resource"),
                    ResourceOrigin::Service
                },
                true
            }
        };
        QVERIFY(bindingRepository.replaceWorkspaceBindings({
            WorkspaceServiceBindingSnapshot{
                QStringLiteral("ws-001"),
                QStringLiteral("Workspace One"),
                groupBindings
            }
        }));
        QVERIFY(bindingRepository.replaceGroupBindings(groupBindings));
        QVERIFY(bindingRepository.saveCurrentSelection(ServiceSelectionSnapshot{
            QStringLiteral("ws-001"),
            QStringLiteral("group-001"),
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("group-binding"),
            {},
            ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712501000000LL, {}},
            {},
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QString(),
                QString(),
                QString(),
                QString(),
                ResourceOrigin::Service
            },
            true
        }));
        QVERIFY(resourceRepository.replaceResources({
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_file"),
                QStringLiteral("Design Spec"),
                QStringLiteral("v1"),
                QStringLiteral("Service resource"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QString(),
                QString(),
                QStringLiteral("local-001"),
                QStringLiteral("draft"),
                QStringLiteral("Local Draft"),
                QStringLiteral("v1"),
                QStringLiteral("Local resource"),
                ResourceOrigin::Local
            }
        }));

        PersistedRuntimeArchitectureLoader loader(connectionName);
        const RuntimeArchitectureSnapshot snapshot = loader.loadSnapshot();

        QCOMPARE(snapshot.discoveryResult.defaultServiceId, QStringLiteral("svc-001"));
        QCOMPARE(snapshot.serviceRegistry.size(), 1);
        QCOMPARE(snapshot.workspaceBindings.size(), 1);
        QCOMPARE(snapshot.groupBindings.size(), 1);
        QCOMPARE(snapshot.selection.groupId, QStringLiteral("group-001"));
        QCOMPARE(snapshot.visibleResources.size(), 2);
    }
};

QTEST_MAIN(TestArchitectureReadiness)
#include "TestArchitectureReadiness.moc"
