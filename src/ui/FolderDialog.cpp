#include "FolderDialog.h"

#include "ActionTableEditor.h"
#include "ItemCheckStyle.h"
#include "model/FolderStore.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {
constexpr int IdRole = Qt::UserRole + 1;
}

FolderDialog::FolderDialog(const FolderStore *store, const Folder &folder, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_folder(folder)
    , m_name(new QLineEdit(folder.name, this))
    , m_path(new QLineEdit(QDir::toNativeSeparators(folder.path), this))
    , m_globalActions(new QListWidget(this))
    , m_customActions(new ActionTableEditor(ActionTableEditor::Mode::Folder, this))
{
    setWindowTitle(tr("Edit folder"));
    resize(640, 520);

    auto *browse = new QPushButton(tr("&Browse…"), this);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Choose folder"),
                                                              m_path->text());
        if (dir.isEmpty())
            return;
        m_path->setText(QDir::toNativeSeparators(dir));
        if (m_name->text().trimmed().isEmpty())
            m_name->setText(QFileInfo(dir).fileName());
    });
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browse);

    auto *form = new QFormLayout;
    form->addRow(tr("&Name:"), m_name);
    form->addRow(tr("&Path:"), pathRow);

    for (const Action &a : store->actions()) {
        const QString label = a.allFolders ? a.name : tr("%1 (optional)").arg(a.name);
        auto *item = new QListWidgetItem(label, m_globalActions);
        item->setData(IdRole, a.id);
        item->setToolTip(a.command);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(FolderStore::isShown(a, folder) ? Qt::Checked : Qt::Unchecked);
    }
    m_globalActions->setMaximumHeight(140);
    ItemCheckStyle::apply(m_globalActions);
    auto *globalBox = new QGroupBox(tr("&Library actions shown on this folder"), this);
    auto *globalLayout = new QVBoxLayout(globalBox);
    globalLayout->addWidget(m_globalActions);

    m_customActions->setActions(folder.customActions);
    auto *customBox = new QGroupBox(tr("Actions for this folder only"), this);
    auto *customLayout = new QVBoxLayout(customBox);
    customLayout->addWidget(m_customActions);
    auto *help = new QLabel(ActionTableEditor::helpText(), customBox);
    help->setWordWrap(true);
    customLayout->addWidget(help);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(globalBox);
    layout->addWidget(customBox, 1);
    layout->addWidget(buttons);
}

Folder FolderDialog::folder() const
{
    Folder f = m_folder;
    f.name = m_name->text().trimmed();
    f.path = QDir::cleanPath(QDir::fromNativeSeparators(m_path->text().trimmed()));
    f.disabledActions.clear();
    f.addedActions.clear();
    for (int i = 0; i < m_globalActions->count(); ++i) {
        const QListWidgetItem *item = m_globalActions->item(i);
        const QUuid id = item->data(IdRole).toUuid();
        const bool checked = item->checkState() == Qt::Checked;
        const auto &library = m_store->actions();
        const auto action = std::find_if(library.cbegin(), library.cend(),
                                         [&id](const Action &a) { return a.id == id; });
        if (action == library.cend())
            continue;
        if (action->allFolders && !checked)
            f.disabledActions.append(id);
        else if (!action->allFolders && checked)
            f.addedActions.append(id);
    }
    f.customActions = m_customActions->actions();
    return f;
}

void FolderDialog::accept()
{
    const Folder f = folder();
    if (f.path.isEmpty() || !QFileInfo(f.path).isDir()) {
        QMessageBox::warning(this, windowTitle(), tr("The path is not an existing folder."));
        m_path->setFocus();
        return;
    }
    const Folder *other = m_store->folderWithPath(f.path);
    if (other && other->id != f.id) {
        QMessageBox::warning(this, windowTitle(),
                             tr("%1 already has this path.").arg(other->name));
        m_path->setFocus();
        return;
    }
    if (f.name.isEmpty()) {
        const QString base = QFileInfo(f.path).fileName();
        m_name->setText(base.isEmpty() ? QDir::toNativeSeparators(f.path) : base);
    }
    QDialog::accept();
}
