#include "ui/KnowledgeServiceSettingsWidget.h"

#include "integrations/KnowServiceClient.h"
#include "ui/AppStyle.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <ElaCheckBox.h>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <ElaFrame.h>
#include <QHBoxLayout>
#include <QPalette>
#include <ElaText.h>
#include <ElaLineEdit.h>
#include <ElaListWidget.h>
#include <QListWidget>
#include <ElaPushButton.h>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

#include "ElaTheme.h"

namespace {

constexpr int kLocalIdRole = Qt::UserRole + 1;

QString trimmedText(const QString& value)
{
    return value.trimmed();
}

QStringList normalizedCapabilitiesFromText(const QString& text)
{
    QString normalized = text;
    normalized.replace(QStringLiteral("，"), QStringLiteral(","));
    normalized.replace(QStringLiteral("；"), QStringLiteral(","));
    normalized.replace(QLatin1Char(';'), QLatin1Char(','));
    normalized.replace(QLatin1Char('\n'), QLatin1Char(','));

    QStringList capabilities;
    const QStringList parts = normalized.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString capability = part.trimmed();
        if (!capability.isEmpty() && !capabilities.contains(capability)) {
            capabilities.push_back(capability);
        }
    }
    return capabilities;
}

QString capabilitiesToText(const QStringList& capabilities)
{
    QStringList normalized;
    for (const QString& capability : capabilities) {
        const QString value = capability.trimmed();
        if (!value.isEmpty() && !normalized.contains(value)) {
            normalized.push_back(value);
        }
    }
    return normalized.join(QStringLiteral(", "));
}

QString colorWithAlpha(QColor color, int alpha)
{
    color.setAlpha(qBound(0, alpha, 255));
    return color.name(QColor::HexArgb);
}

KnowledgeServiceConfig normalizedConfig(KnowledgeServiceConfig config)
{
    config.localId = trimmedText(config.localId);
    config.displayName = trimmedText(config.displayName);
    config.baseUrl = trimmedText(config.baseUrl);
    config.teamLabel = trimmedText(config.teamLabel);
    config.accessToken = trimmedText(config.accessToken);
    config.serviceInstanceId = trimmedText(config.serviceInstanceId);
    config.knowledgeBaseId = trimmedText(config.knowledgeBaseId);
    config.apiVersion = trimmedText(config.apiVersion);
    config.lastConnectionMessage = trimmedText(config.lastConnectionMessage);
    config.capabilities = normalizedCapabilitiesFromText(capabilitiesToText(config.capabilities));
    return config;
}

bool isValidKnowledgeServiceUrl(const QString& urlText)
{
    const QUrl url(urlText.trimmed());
    if (!url.isValid()) {
        return false;
    }
    const QString scheme = url.scheme().trimmed().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
        return false;
    }
    return !url.host().trimmed().isEmpty();
}

QString connectionSuccessMessage(const KnowServiceQueryResponse& response)
{
    QStringList parts;
    if (!response.service.serviceInstanceId.trimmed().isEmpty()) {
        parts.push_back(response.service.serviceInstanceId.trimmed());
    }
    if (!response.service.knowledgeBaseId.trimmed().isEmpty()) {
        parts.push_back(response.service.knowledgeBaseId.trimmed());
    }
    if (!response.service.apiVersion.trimmed().isEmpty()) {
        parts.push_back(response.service.apiVersion.trimmed());
    }
    if (!response.service.capabilities.isEmpty()) {
        parts.push_back(response.service.capabilities.join(QStringLiteral(", ")));
    }

    return parts.isEmpty()
        ? QStringLiteral("连接成功：服务可达")
        : QStringLiteral("连接成功：%1").arg(parts.join(QStringLiteral(" / ")));
}

void applyConnectionMetadata(KnowledgeServiceConfig& config, const KnowServiceServiceMeta& service)
{
    if (!service.serviceInstanceId.trimmed().isEmpty()) {
        config.serviceInstanceId = service.serviceInstanceId.trimmed();
    }
    if (!service.knowledgeBaseId.trimmed().isEmpty()) {
        config.knowledgeBaseId = service.knowledgeBaseId.trimmed();
    }
    if (!service.apiVersion.trimmed().isEmpty()) {
        config.apiVersion = service.apiVersion.trimmed();
    }
    if (!service.capabilities.isEmpty()) {
        config.capabilities = service.capabilities;
    }
}

}

KnowledgeServiceSettingsWidget::KnowledgeServiceSettingsWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("knowledgeServiceSettingsWidget"));

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* listCard = new ElaFrame(this);
    listCard->setObjectName(QStringLiteral("knowledgeServiceListCard"));
    m_listCard = listCard;
    auto* listLayout = new QVBoxLayout(listCard);
    listLayout->setContentsMargins(12, 12, 12, 12);
    listLayout->setSpacing(10);

    auto* listTitle = new ElaText(QStringLiteral("知识服务列表"), listCard);
    listTitle->setObjectName(QStringLiteral("knowledgeServiceListTitle"));
    m_listTitle = listTitle;
    auto* listHint = new ElaText(QStringLiteral("支持拖拽排序、复制配置，以及批量导入导出。"), listCard);
    listHint->setWordWrap(true);
    listHint->setObjectName(QStringLiteral("knowledgeServiceListHint"));
    m_listHint = listHint;
    m_serviceList = new ElaListWidget(listCard);
    m_serviceList->setObjectName(QStringLiteral("knowledgeServiceList"));
    m_serviceList->setMinimumWidth(240);
    m_serviceList->setDragDropMode(QAbstractItemView::InternalMove);
    m_serviceList->setDefaultDropAction(Qt::MoveAction);
    m_serviceList->setDragEnabled(true);
    m_serviceList->setAcceptDrops(true);
    m_serviceList->setDropIndicatorShown(true);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(8);
    m_addButton = new ElaPushButton(QStringLiteral("新增服务"), listCard);
    m_addButton->setObjectName(QStringLiteral("knowledgeServiceAddBtn"));
    m_duplicateButton = new ElaPushButton(QStringLiteral("复制服务"), listCard);
    m_duplicateButton->setObjectName(QStringLiteral("knowledgeServiceDuplicateBtn"));
    m_removeButton = new ElaPushButton(QStringLiteral("删除选中"), listCard);
    m_removeButton->setObjectName(QStringLiteral("knowledgeServiceRemoveBtn"));
    buttonRow->addWidget(m_addButton);
    buttonRow->addWidget(m_duplicateButton);
    buttonRow->addWidget(m_removeButton);

    auto* secondaryButtonRow = new QHBoxLayout;
    secondaryButtonRow->setContentsMargins(0, 0, 0, 0);
    secondaryButtonRow->setSpacing(8);
    m_importButton = new ElaPushButton(QStringLiteral("批量导入"), listCard);
    m_importButton->setObjectName(QStringLiteral("knowledgeServiceImportBtn"));
    m_exportButton = new ElaPushButton(QStringLiteral("批量导出"), listCard);
    m_exportButton->setObjectName(QStringLiteral("knowledgeServiceExportBtn"));
    secondaryButtonRow->addWidget(m_importButton);
    secondaryButtonRow->addWidget(m_exportButton);
    secondaryButtonRow->addStretch();

    listLayout->addWidget(listTitle);
    listLayout->addWidget(listHint);
    listLayout->addWidget(m_serviceList, 1);
    listLayout->addLayout(buttonRow);
    listLayout->addLayout(secondaryButtonRow);

    auto* editorCard = new ElaFrame(this);
    editorCard->setObjectName(QStringLiteral("knowledgeServiceEditorCard"));
    m_editorCard = editorCard;
    auto* editorLayout = new QVBoxLayout(editorCard);
    editorLayout->setContentsMargins(14, 14, 14, 14);
    editorLayout->setSpacing(12);

    auto* editorTitle = new ElaText(QStringLiteral("服务详情"), editorCard);
    editorTitle->setObjectName(QStringLiteral("knowledgeServiceEditorTitle"));
    m_editorTitle = editorTitle;
    auto* editorHint = new ElaText(
        QStringLiteral("至少需要填写服务名称和服务地址。可以先做连接测试，再决定是否保存。"),
        editorCard);
    editorHint->setWordWrap(true);
    editorHint->setObjectName(QStringLiteral("knowledgeServiceEditorHint"));
    m_editorHint = editorHint;

    auto* formLayout = new QFormLayout;
    m_formLayout = formLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(8);
    formLayout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    const auto createLineEdit = [editorCard](const QString& objectName, const QString& placeholder) {
        auto* edit = new ElaLineEdit(editorCard);
        edit->setObjectName(objectName);
        edit->setPlaceholderText(placeholder);
        return edit;
    };

    m_displayNameEdit = createLineEdit(QStringLiteral("knowledgeServiceDisplayNameEdit"),
                                       QStringLiteral("例如：研发知识库"));
    m_baseUrlEdit = createLineEdit(QStringLiteral("knowledgeServiceBaseUrlEdit"),
                                   QStringLiteral("例如：https://knowservice.example.com"));
    m_teamLabelEdit = createLineEdit(QStringLiteral("knowledgeServiceTeamLabelEdit"),
                                     QStringLiteral("例如：工业软件中心"));
    m_accessTokenEdit = createLineEdit(QStringLiteral("knowledgeServiceAccessTokenEdit"),
                                       QStringLiteral("可选，留空表示匿名访问"));
    m_accessTokenEdit->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    m_serviceInstanceIdEdit = createLineEdit(QStringLiteral("knowledgeServiceServiceInstanceEdit"),
                                             QStringLiteral("连接测试成功后可自动回填"));
    m_knowledgeBaseIdEdit = createLineEdit(QStringLiteral("knowledgeServiceKnowledgeBaseEdit"),
                                           QStringLiteral("连接测试成功后可自动回填"));
    m_apiVersionEdit = createLineEdit(QStringLiteral("knowledgeServiceApiVersionEdit"),
                                      QStringLiteral("例如：v1"));
    m_capabilitiesEdit = createLineEdit(QStringLiteral("knowledgeServiceCapabilitiesEdit"),
                                        QStringLiteral("可选，多个能力用逗号分隔"));
    m_defaultCheck = new ElaCheckBox(QStringLiteral("设为默认知识服务"), editorCard);
    m_defaultCheck->setObjectName(QStringLiteral("knowledgeServiceDefaultCheck"));
    m_testConnectionButton = new ElaPushButton(QStringLiteral("测试连接"), editorCard);
    m_testConnectionButton->setObjectName(QStringLiteral("knowledgeServiceTestConnectionBtn"));
    m_statusLabel = new ElaText(editorCard);
    m_statusLabel->setObjectName(QStringLiteral("knowledgeServiceStatusLabel"));
    m_statusLabel->setWordWrap(true);

    formLayout->addRow(QStringLiteral("服务名称"), m_displayNameEdit);
    formLayout->addRow(QStringLiteral("服务地址"), m_baseUrlEdit);
    formLayout->addRow(QStringLiteral("团队标签"), m_teamLabelEdit);
    formLayout->addRow(QStringLiteral("访问令牌"), m_accessTokenEdit);
    formLayout->addRow(QStringLiteral("实例 ID"), m_serviceInstanceIdEdit);
    formLayout->addRow(QStringLiteral("知识库 ID"), m_knowledgeBaseIdEdit);
    formLayout->addRow(QStringLiteral("API 版本"), m_apiVersionEdit);
    formLayout->addRow(QStringLiteral("能力标签"), m_capabilitiesEdit);
    formLayout->addRow(QString(), m_defaultCheck);
    formLayout->addRow(QString(), m_testConnectionButton);

    editorLayout->addWidget(editorTitle);
    editorLayout->addWidget(editorHint);
    editorLayout->addLayout(formLayout);
    editorLayout->addWidget(m_statusLabel);
    editorLayout->addStretch();

    rootLayout->addWidget(listCard, 0);
    rootLayout->addWidget(editorCard, 1);

    connect(m_addButton, &ElaPushButton::clicked, this, [this]() { addService(); });
    connect(m_duplicateButton, &ElaPushButton::clicked, this, [this]() { duplicateCurrentService(); });
    connect(m_removeButton, &ElaPushButton::clicked, this, [this]() { removeCurrentService(); });
    connect(m_testConnectionButton, &ElaPushButton::clicked, this, [this]() { testCurrentConnection(); });
    connect(m_importButton, &ElaPushButton::clicked, this, [this]() {
        const QString filePath = QFileDialog::getOpenFileName(
            this,
            QStringLiteral("选择知识服务配置文件"),
            QString(),
            QStringLiteral("JSON 文件 (*.json)"));
        if (filePath.trimmed().isEmpty()) {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            setStatusText(QStringLiteral("导入失败：无法读取文件。"));
            return;
        }

        QString errorMessage;
        if (!importFromJson(QString::fromUtf8(file.readAll()), &errorMessage)) {
            setStatusText(errorMessage.trimmed().isEmpty()
                              ? QStringLiteral("导入失败：文件中没有可用的知识服务配置。")
                              : errorMessage);
            return;
        }

        setStatusText(QStringLiteral("已导入知识服务配置。"));
    });
    connect(m_exportButton, &ElaPushButton::clicked, this, [this]() {
        const QString json = exportToJson();
        if (json.trimmed().isEmpty()) {
            setStatusText(QStringLiteral("当前没有可导出的知识服务配置。"));
            return;
        }

        const QString filePath = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("导出知识服务配置"),
            QStringLiteral("knowledge-services.json"),
            QStringLiteral("JSON 文件 (*.json)"));
        if (filePath.trimmed().isEmpty()) {
            return;
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            setStatusText(QStringLiteral("导出失败：无法写入目标文件。"));
            return;
        }

        file.write(json.toUtf8());
        file.close();
        setStatusText(QStringLiteral("已导出知识服务配置。"));
    });
    connect(m_serviceList, &QListWidget::currentRowChanged, this, [this](int currentRow) {
        handleCurrentRowChanged(currentRow);
    });
    connect(m_serviceList->model(),
            &QAbstractItemModel::rowsMoved,
            this,
            [this](const QModelIndex&, int, int, const QModelIndex&, int) {
                syncConfigsFromListOrder();
            });

    const auto bindTextChange = [this](QLineEdit* edit) {
        connect(edit, &ElaLineEdit::textChanged, this, [this](const QString&) {
            syncCurrentFieldsToModel();
            refreshServiceList();
        });
    };
    bindTextChange(m_displayNameEdit);
    bindTextChange(m_baseUrlEdit);
    bindTextChange(m_teamLabelEdit);
    bindTextChange(m_accessTokenEdit);
    bindTextChange(m_serviceInstanceIdEdit);
    bindTextChange(m_knowledgeBaseIdEdit);
    bindTextChange(m_apiVersionEdit);
    bindTextChange(m_capabilitiesEdit);
    connect(m_defaultCheck, &ElaCheckBox::toggled, this, [this](bool) {
        syncCurrentFieldsToModel();
        refreshServiceList();
    });

    setEditorsEnabled(false);
    setStatusText(QStringLiteral("当前未配置知识服务，可先新增一项。"));
    refreshStyles();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]() { refreshStyles(); });
}

void KnowledgeServiceSettingsWidget::setConfigs(const QVector<KnowledgeServiceConfig>& configs)
{
    m_configs.clear();

    bool defaultAssigned = false;
    for (KnowledgeServiceConfig config : configs) {
        config = normalizedConfig(config);
        if (config.localId.isEmpty()) {
            config.localId = nextGeneratedLocalId();
        }
        if (config.isDefault) {
            if (defaultAssigned) {
                config.isDefault = false;
            } else {
                defaultAssigned = true;
            }
        }
        m_configs.push_back(config);
    }

    refreshServiceList();
    if (m_configs.isEmpty()) {
        handleCurrentRowChanged(-1);
        return;
    }

    int selectedRow = 0;
    for (int index = 0; index < m_configs.size(); ++index) {
        if (m_configs.at(index).isDefault) {
            selectedRow = index;
            break;
        }
    }
    m_serviceList->setCurrentRow(selectedRow);
    m_updatingUi = true;
    handleCurrentRowChanged(selectedRow);
}

QVector<KnowledgeServiceConfig> KnowledgeServiceSettingsWidget::configs()
{
    syncCurrentFieldsToModel();

    QVector<KnowledgeServiceConfig> normalized;
    bool defaultAssigned = false;
    for (KnowledgeServiceConfig config : m_configs) {
        config = normalizedConfig(config);
        if (config.isDefault) {
            if (defaultAssigned) {
                config.isDefault = false;
            } else {
                defaultAssigned = true;
            }
        }
        normalized.push_back(config);
    }
    m_configs = normalized;

    // 如果所有服务都未设为默认，自动将第一个标记为默认
    if (!m_configs.isEmpty()) {
        bool hasDefault = false;
        for (const KnowledgeServiceConfig& config : std::as_const(m_configs)) {
            if (config.isDefault) {
                hasDefault = true;
                break;
            }
        }
        if (!hasDefault) {
            m_configs[0].isDefault = true;
        }
    }

    return m_configs;
}

bool KnowledgeServiceSettingsWidget::validate(QString* errorMessage)
{
    const QVector<KnowledgeServiceConfig> currentConfigs = configs();
    for (int index = 0; index < currentConfigs.size(); ++index) {
        const KnowledgeServiceConfig& config = currentConfigs.at(index);
        const QString serviceLabel = serviceDisplayLabel(config, index + 1);
        if (config.localId.trimmed().isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("知识服务“%1”缺少内部标识，请删除后重新新增。")
                                    .arg(serviceLabel);
            }
            return false;
        }
        if (config.displayName.trimmed().isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("请为知识服务“%1”填写服务名称。")
                                    .arg(serviceLabel);
            }
            return false;
        }
        if (config.baseUrl.trimmed().isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("请为知识服务“%1”填写服务地址。")
                                    .arg(serviceLabel);
            }
            return false;
        }
        if (!isValidKnowledgeServiceUrl(config.baseUrl)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("知识服务“%1”的服务地址无效，请填写 http 或 https 地址。")
                                    .arg(serviceLabel);
            }
            return false;
        }
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void KnowledgeServiceSettingsWidget::setConnectionClient(KnowServiceClient* client)
{
    if (m_ownsConnectionClient && m_connectionClient && m_connectionClient != client) {
        m_connectionClient->deleteLater();
    }
    m_connectionClient = client;
    m_ownsConnectionClient = false;
}

QString KnowledgeServiceSettingsWidget::exportToJson()
{
    return KnowledgeServiceSettingsStore::toJson(configs());
}

bool KnowledgeServiceSettingsWidget::importFromJson(const QString& json, QString* errorMessage)
{
    const QVector<KnowledgeServiceConfig> importedConfigs = KnowledgeServiceSettingsStore::fromJson(json);
    if (importedConfigs.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("导入失败：未解析到任何可用的知识服务配置。");
        }
        return false;
    }

    syncCurrentFieldsToModel();

    bool hasDefault = false;
    for (const KnowledgeServiceConfig& config : m_configs) {
        if (config.isDefault) {
            hasDefault = true;
            break;
        }
    }

    for (KnowledgeServiceConfig config : importedConfigs) {
        config = normalizedConfig(config);
        if (config.localId.isEmpty() || indexOfLocalId(config.localId) >= 0) {
            config.localId = nextGeneratedLocalId();
        }
        if (hasDefault) {
            config.isDefault = false;
        } else if (config.isDefault) {
            hasDefault = true;
        }
        m_configs.push_back(config);
    }

    refreshServiceList();
    const int newRow = qMax(0, m_configs.size() - importedConfigs.size());
    m_serviceList->setCurrentRow(newRow);
    m_updatingUi = true;
    handleCurrentRowChanged(newRow);
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool KnowledgeServiceSettingsWidget::moveService(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_configs.size() || toIndex < 0 || toIndex >= m_configs.size()) {
        return false;
    }
    if (fromIndex == toIndex) {
        return true;
    }

    syncCurrentFieldsToModel();
    const KnowledgeServiceConfig moved = m_configs.takeAt(fromIndex);
    m_configs.insert(toIndex, moved);
    refreshServiceList();
    m_serviceList->setCurrentRow(toIndex);
    m_updatingUi = true;
    handleCurrentRowChanged(toIndex);
    return true;
}

void KnowledgeServiceSettingsWidget::addService()
{
    syncCurrentFieldsToModel();

    KnowledgeServiceConfig config;
    config.localId = nextGeneratedLocalId();
    config.displayName = QStringLiteral("新知识服务 %1").arg(m_configs.size() + 1);
    config.apiVersion = QStringLiteral("v1");
    config.isDefault = m_configs.isEmpty();
    m_configs.push_back(config);
    refreshServiceList();
    const int newRow = m_configs.size() - 1;
    m_serviceList->setCurrentRow(newRow);
    m_updatingUi = true;
    handleCurrentRowChanged(newRow);
}

void KnowledgeServiceSettingsWidget::duplicateCurrentService()
{
    if (m_currentRow < 0 || m_currentRow >= m_configs.size()) {
        return;
    }

    syncCurrentFieldsToModel();
    KnowledgeServiceConfig copied = m_configs.at(m_currentRow);
    copied.localId = nextGeneratedLocalId();
    copied.displayName = nextDuplicateDisplayName(copied.displayName);
    copied.isDefault = false;
    m_configs.insert(m_currentRow + 1, copied);
    refreshServiceList();
    const int newRow = m_currentRow + 1;
    m_serviceList->setCurrentRow(newRow);
    m_updatingUi = true;
    handleCurrentRowChanged(newRow);
}

void KnowledgeServiceSettingsWidget::removeCurrentService()
{
    if (m_currentRow < 0 || m_currentRow >= m_configs.size()) {
        return;
    }

    const bool removingDefault = m_configs.at(m_currentRow).isDefault;
    m_configs.removeAt(m_currentRow);
    if (removingDefault && !m_configs.isEmpty()) {
        bool hasDefault = false;
        for (const KnowledgeServiceConfig& config : m_configs) {
            if (config.isDefault) {
                hasDefault = true;
                break;
            }
        }
        if (!hasDefault) {
            m_configs[0].isDefault = true;
        }
    }

    refreshServiceList();
    if (m_configs.isEmpty()) {
        m_updatingUi = true;
        handleCurrentRowChanged(-1);
        return;
    }
    const int newRow = qMin(m_currentRow, m_configs.size() - 1);
    m_serviceList->setCurrentRow(newRow);
    m_updatingUi = true;
    handleCurrentRowChanged(newRow);
}

void KnowledgeServiceSettingsWidget::testCurrentConnection()
{
    if (m_currentRow < 0 || m_currentRow >= m_configs.size()) {
        setStatusText(QStringLiteral("请先选择要测试的知识服务。"));
        return;
    }

    syncCurrentFieldsToModel();
    const KnowledgeServiceConfig config = normalizedConfig(m_configs.at(m_currentRow));
    if (config.displayName.isEmpty()) {
        setStatusText(QStringLiteral("请先填写服务名称，再进行连接测试。"));
        m_displayNameEdit->setFocus();
        return;
    }
    if (config.baseUrl.isEmpty()) {
        setStatusText(QStringLiteral("请先填写服务地址，再进行连接测试。"));
        m_baseUrlEdit->setFocus();
        return;
    }
    if (!isValidKnowledgeServiceUrl(config.baseUrl)) {
        setStatusText(QStringLiteral("服务地址无效，请填写 http 或 https 地址后再测试。"));
        m_baseUrlEdit->setFocus();
        return;
    }

    KnowServiceClient* client = ensureConnectionClient();
    if (!client) {
        setStatusText(QStringLiteral("当前无法创建知识服务客户端。"));
        return;
    }

    const QString localId = config.localId;
    m_testConnectionButton->setEnabled(false);
    setStatusText(QStringLiteral("正在测试连接，请稍候..."));
    client->testConnection(config, [this, localId](KnowServiceQueryResponse response) {
        const int index = indexOfLocalId(localId);
        if (index < 0) {
            if (m_testConnectionButton) {
                m_testConnectionButton->setEnabled(true);
            }
            return;
        }

        KnowledgeServiceConfig& configRef = m_configs[index];
        if (!response.errorMessage.trimmed().isEmpty()) {
            configRef.lastConnectionOk = false;
            configRef.lastConnectionMessage = response.errorMessage.trimmed();
        } else {
            configRef.lastConnectionOk = true;
            applyConnectionMetadata(configRef, response.service);
            configRef.lastConnectionMessage = connectionSuccessMessage(response);
        }

        refreshServiceList();
        const int currentIndex = indexOfLocalId(localId);
        if (currentIndex >= 0 && m_serviceList && m_serviceList->currentRow() == currentIndex) {
            m_currentRow = currentIndex;
            loadCurrentModelToFields();
            setStatusText(configRef.lastConnectionMessage);
        }

        if (m_testConnectionButton) {
            m_testConnectionButton->setEnabled(true);
        }
    });
}

void KnowledgeServiceSettingsWidget::handleCurrentRowChanged(int currentRow)
{
    syncCurrentFieldsToModel();
    m_currentRow = currentRow;
    loadCurrentModelToFields();
    setEditorsEnabled(m_currentRow >= 0 && m_currentRow < m_configs.size());

    if (m_currentRow < 0 || m_currentRow >= m_configs.size()) {
        setStatusText(QStringLiteral("当前未配置知识服务，可先新增一项。"));
        return;
    }

    const KnowledgeServiceConfig& config = m_configs.at(m_currentRow);
    QString status = QStringLiteral("服务标识：%1").arg(config.localId);
    if (!config.lastConnectionMessage.trimmed().isEmpty()) {
        status = config.lastConnectionOk
                     ? QStringLiteral("最近连接成功：%1").arg(config.lastConnectionMessage)
                     : QStringLiteral("最近连接状态：%1").arg(config.lastConnectionMessage);
    }
    setStatusText(status);
}

void KnowledgeServiceSettingsWidget::syncCurrentFieldsToModel()
{
    if (m_updatingUi || m_currentRow < 0 || m_currentRow >= m_configs.size()) {
        return;
    }

    KnowledgeServiceConfig& config = m_configs[m_currentRow];
    config.displayName = m_displayNameEdit->text().trimmed();
    config.baseUrl = m_baseUrlEdit->text().trimmed();
    config.teamLabel = m_teamLabelEdit->text().trimmed();
    config.accessToken = m_accessTokenEdit->text().trimmed();
    config.serviceInstanceId = m_serviceInstanceIdEdit->text().trimmed();
    config.knowledgeBaseId = m_knowledgeBaseIdEdit->text().trimmed();
    config.apiVersion = m_apiVersionEdit->text().trimmed();
    config.capabilities = normalizedCapabilitiesFromText(m_capabilitiesEdit->text());
    config.isDefault = m_defaultCheck->isChecked();

    if (config.isDefault) {
        for (int index = 0; index < m_configs.size(); ++index) {
            if (index != m_currentRow) {
                m_configs[index].isDefault = false;
            }
        }
    }
}

void KnowledgeServiceSettingsWidget::loadCurrentModelToFields()
{
    m_updatingUi = true;

    if (m_currentRow < 0 || m_currentRow >= m_configs.size()) {
        m_displayNameEdit->clear();
        m_baseUrlEdit->clear();
        m_teamLabelEdit->clear();
        m_accessTokenEdit->clear();
        m_serviceInstanceIdEdit->clear();
        m_knowledgeBaseIdEdit->clear();
        m_apiVersionEdit->clear();
        m_capabilitiesEdit->clear();
        m_defaultCheck->setChecked(false);
        m_updatingUi = false;
        return;
    }

    const KnowledgeServiceConfig& config = m_configs.at(m_currentRow);
    m_displayNameEdit->setText(config.displayName);
    m_baseUrlEdit->setText(config.baseUrl);
    m_teamLabelEdit->setText(config.teamLabel);
    m_accessTokenEdit->setText(config.accessToken);
    m_serviceInstanceIdEdit->setText(config.serviceInstanceId);
    m_knowledgeBaseIdEdit->setText(config.knowledgeBaseId);
    m_apiVersionEdit->setText(config.apiVersion);
    m_capabilitiesEdit->setText(capabilitiesToText(config.capabilities));
    m_defaultCheck->setChecked(config.isDefault);

    m_updatingUi = false;
}

void KnowledgeServiceSettingsWidget::refreshServiceList()
{
    const QSignalBlocker blocker(m_serviceList);
    m_serviceList->clear();

    for (int index = 0; index < m_configs.size(); ++index) {
        const KnowledgeServiceConfig& config = m_configs.at(index);
        auto* item = new QListWidgetItem(serviceDisplayLabel(config, index + 1), m_serviceList);
        item->setData(kLocalIdRole, config.localId);
    }

    if (!m_configs.isEmpty()) {
        const int safeRow = qBound(0, m_currentRow < 0 ? 0 : m_currentRow, m_configs.size() - 1);
        m_serviceList->setCurrentRow(safeRow);
    }

    const bool hasCurrent = !m_configs.isEmpty() && m_currentRow >= 0 && m_currentRow < m_configs.size();
    m_duplicateButton->setEnabled(hasCurrent);
    m_removeButton->setEnabled(hasCurrent);
    m_exportButton->setEnabled(!m_configs.isEmpty());
}

void KnowledgeServiceSettingsWidget::syncConfigsFromListOrder()
{
    if (m_reorderingFromUi || !m_serviceList) {
        return;
    }

    syncCurrentFieldsToModel();
    const QString currentLocalId = m_serviceList->currentItem()
        ? m_serviceList->currentItem()->data(kLocalIdRole).toString().trimmed()
        : QString();

    QVector<KnowledgeServiceConfig> reordered;
    reordered.reserve(m_serviceList->count());
    for (int row = 0; row < m_serviceList->count(); ++row) {
        const QListWidgetItem* item = m_serviceList->item(row);
        if (!item) {
            continue;
        }

        const int configIndex = indexOfLocalId(item->data(kLocalIdRole).toString().trimmed());
        if (configIndex >= 0) {
            reordered.push_back(m_configs.at(configIndex));
        }
    }

    if (reordered.size() != m_configs.size()) {
        return;
    }

    m_configs = reordered;
    m_currentRow = currentLocalId.isEmpty() ? m_serviceList->currentRow() : indexOfLocalId(currentLocalId);
    loadCurrentModelToFields();
    setEditorsEnabled(m_currentRow >= 0 && m_currentRow < m_configs.size());
}

void KnowledgeServiceSettingsWidget::setEditorsEnabled(bool enabled)
{
    m_displayNameEdit->setEnabled(enabled);
    m_baseUrlEdit->setEnabled(enabled);
    m_teamLabelEdit->setEnabled(enabled);
    m_accessTokenEdit->setEnabled(enabled);
    m_serviceInstanceIdEdit->setEnabled(enabled);
    m_knowledgeBaseIdEdit->setEnabled(enabled);
    m_apiVersionEdit->setEnabled(enabled);
    m_capabilitiesEdit->setEnabled(enabled);
    m_defaultCheck->setEnabled(enabled);
    m_duplicateButton->setEnabled(enabled && !m_configs.isEmpty());
    m_removeButton->setEnabled(enabled && !m_configs.isEmpty());
    m_testConnectionButton->setEnabled(enabled);
}

void KnowledgeServiceSettingsWidget::setStatusText(const QString& message)
{
    m_statusLabel->setText(message.trimmed());
}

void KnowledgeServiceSettingsWidget::refreshStyles()
{
    const AppStyle::ThemeMode mode = AppStyle::themeModeFromEla(eTheme->getThemeMode());
    const bool dark = AppStyle::isDarkTheme(mode);
    const QString srf = dark ? QStringLiteral("#bc181f27") : QStringLiteral("#96ffffff");
    const QString brd = colorWithAlpha(QColor(AppStyle::border(mode)), dark ? 128 : 96);
    const QString brdHover = colorWithAlpha(QColor(AppStyle::borderStrong(mode)), dark ? 190 : 140);
    const QString sAlt = dark ? QStringLiteral("#b0121a22") : QStringLiteral("#76ffffff");
    const QString sMuted = dark ? QStringLiteral("#9c141b23") : QStringLiteral("#68ffffff");
    const QString hoverBg = dark ? QStringLiteral("#c01f2b37") : QStringLiteral("#aaffffff");
    const QString txt = AppStyle::textPrimary(mode);
    const QString txtM = AppStyle::textMuted(mode);
    const QString selBg = colorWithAlpha(QColor(AppStyle::accent(mode)), dark ? 74 : 48);
    const QString accent = AppStyle::accent(mode);
    const QString inputHoverBg = dark ? QStringLiteral("#c0182530") : QStringLiteral("#a6ffffff");

    if (m_listCard) {
        m_listCard->setStyleSheet(QStringLiteral(
            "QFrame#knowledgeServiceListCard {"
            "  background:%1; border:1px solid %2; border-radius:10px;"
            "}"
            "QListWidget#knowledgeServiceList {"
            "  border:1px solid %2; border-radius:8px; background:%3; color:%4;"
            "  padding:4px; outline:none;"
            "}"
            "QListWidget#knowledgeServiceList::item {"
            "  padding:8px 10px; border-radius:6px; color:%4;"
            "}"
            "QListWidget#knowledgeServiceList::item:hover {"
            "  background:%6;"
            "}"
            "QListWidget#knowledgeServiceList::item:selected {"
            "  background:%5; color:%4;"
            "}"
            "QPushButton {"
            "  background:%3; color:%4; border:1px solid %2;"
            "  border-radius:6px; padding:6px 10px; font-weight:600;"
            "}"
            "QPushButton:hover {"
            "  background:%6; border-color:%7;"
            "}"
            "QPushButton:disabled {"
            "  background:%8; color:%9; border-color:%2;"
            "}")
            .arg(srf, brd, sAlt, txt, selBg, hoverBg, brdHover, sMuted, txtM));
        if (m_serviceList && m_serviceList->viewport()) {
            m_serviceList->viewport()->setAutoFillBackground(false);
            QPalette palette = m_serviceList->viewport()->palette();
            palette.setColor(QPalette::Base, Qt::transparent);
            palette.setColor(QPalette::Window, Qt::transparent);
            m_serviceList->viewport()->setPalette(palette);
        }
    }
    if (m_editorCard) {
        m_editorCard->setStyleSheet(QStringLiteral(
            "QFrame#knowledgeServiceEditorCard {"
            "  background:%1; border:1px solid %2; border-radius:10px;"
            "}"
            "QLineEdit {"
            "  background:%3; color:%4; border:1px solid %2;"
            "  border-radius:6px; padding:7px 10px; font-size:13px;"
            "}"
            "QLineEdit:hover {"
            "  background:%9; border-color:%7;"
            "}"
            "QLineEdit:focus {"
            "  background:%9; border-color:%8;"
            "}"
            "QLineEdit:disabled {"
            "  background:%6; color:%5; border-color:%2;"
            "}"
            "QLineEdit::placeholder {"
            "  color:%5;"
            "}"
            "QCheckBox {"
            "  color:%4; font-size:13px;"
            "}"
            "QCheckBox:disabled {"
            "  color:%5;"
            "}"
            "QPushButton {"
            "  background:%3; color:%4; border:1px solid %2;"
            "  border-radius:6px; padding:7px 12px; font-weight:600;"
            "}"
            "QPushButton:hover {"
            "  background:%9; border-color:%7;"
            "}"
            "QPushButton:disabled {"
            "  background:%6; color:%5; border-color:%2;"
            "}")
            .arg(srf, brd, sAlt, txt, txtM, sMuted, brdHover, accent, inputHoverBg));
    }
    if (m_listTitle) {
        m_listTitle->setStyleSheet(QStringLiteral("font-size:13px; font-weight:700; color:%1;").arg(txt));
    }
    if (m_listHint) {
        m_listHint->setStyleSheet(QStringLiteral("font-size:12px; color:%1;").arg(txtM));
    }
    if (m_editorTitle) {
        m_editorTitle->setStyleSheet(QStringLiteral("font-size:13px; font-weight:700; color:%1;").arg(txt));
    }
    if (m_editorHint) {
        m_editorHint->setStyleSheet(QStringLiteral("font-size:12px; color:%1;").arg(txtM));
    }
    if (m_statusLabel) {
        m_statusLabel->setStyleSheet(QStringLiteral("font-size:12px; color:%1;").arg(txtM));
    }
    // QFormLayout 行标签（原生 QLabel）深色模式适配
    if (m_formLayout) {
        for (int row = 0; row < m_formLayout->rowCount(); ++row) {
            QLayoutItem* labelItem = m_formLayout->itemAt(row, QFormLayout::LabelRole);
            if (labelItem) {
                if (auto* label = qobject_cast<QLabel*>(labelItem->widget())) {
                    label->setStyleSheet(QStringLiteral("color:%1; font-size:13px;").arg(txt));
                }
            }
        }
    }
}

QString KnowledgeServiceSettingsWidget::nextGeneratedLocalId() const
{
    int nextIndex = 1;
    while (true) {
        const QString candidate = QStringLiteral("knowledge-service-%1").arg(nextIndex);
        bool exists = false;
        for (const KnowledgeServiceConfig& config : m_configs) {
            if (config.localId.compare(candidate, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            return candidate;
        }
        ++nextIndex;
    }
}

QString KnowledgeServiceSettingsWidget::nextDuplicateDisplayName(const QString& sourceName) const
{
    const QString baseName = sourceName.trimmed().isEmpty()
        ? QStringLiteral("新知识服务")
        : sourceName.trimmed();
    return QStringLiteral("%1 副本").arg(baseName);
}

QString KnowledgeServiceSettingsWidget::serviceDisplayLabel(const KnowledgeServiceConfig& config, int index) const
{
    QString label = config.displayName.trimmed();
    if (label.isEmpty()) {
        label = config.baseUrl.trimmed();
    }
    if (label.isEmpty()) {
        label = QStringLiteral("未命名服务 %1").arg(index);
    }
    if (config.isDefault) {
        label.append(QStringLiteral("（默认）"));
    }
    return label;
}

int KnowledgeServiceSettingsWidget::indexOfLocalId(const QString& localId) const
{
    const QString normalizedLocalId = localId.trimmed();
    if (normalizedLocalId.isEmpty()) {
        return -1;
    }

    for (int index = 0; index < m_configs.size(); ++index) {
        if (m_configs.at(index).localId.compare(normalizedLocalId, Qt::CaseInsensitive) == 0) {
            return index;
        }
    }
    return -1;
}

KnowServiceClient* KnowledgeServiceSettingsWidget::ensureConnectionClient()
{
    if (m_connectionClient) {
        return m_connectionClient;
    }

    m_connectionClient = new HttpKnowServiceClient(this);
    m_ownsConnectionClient = true;
    return m_connectionClient;
}
