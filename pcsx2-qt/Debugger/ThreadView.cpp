// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "ThreadView.h"

#include "QtUtils.h"

#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>

ThreadView::ThreadView(const DebuggerViewParameters& parameters)
	: DebuggerView(parameters, MONOSPACE_FONT)
	, m_model(new ThreadModel(cpu()))
	, m_proxy_model(new QSortFilterProxyModel())
{
	m_ui.setupUi(this);

	m_ui.threadList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_ui.threadList, &QTableView::customContextMenuRequested, this, &ThreadView::openContextMenu);
	connect(m_ui.threadList, &QTableView::doubleClicked, this, &ThreadView::onDoubleClick);

	m_proxy_model->setSourceModel(m_model);
	m_proxy_model->setSortRole(Qt::UserRole);
	m_ui.threadList->setModel(m_proxy_model);
	m_ui.threadList->setSortingEnabled(true);
	m_ui.threadList->sortByColumn(ThreadModel::ThreadColumns::ID, Qt::SortOrder::AscendingOrder);
	m_ui.threadList->horizontalHeader()->setSectionsMovable(true);
	m_ui.threadList->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeMode::ResizeToContents);
	for (std::size_t i = 0; auto mode : ThreadModel::HeaderResizeModes)
	{
		m_ui.threadList->horizontalHeader()->setSectionResizeMode(i, mode);
		i++;
	}

	receiveEvent<DebuggerEvents::Refresh>([this](const DebuggerEvents::Refresh& event) -> bool {
		if (!QtHost::IsVMPaused())
			m_model->refreshData();
		return true;
	});

	receiveEvent<DebuggerEvents::VMUpdate>([this](const DebuggerEvents::VMUpdate& event) -> bool {
		m_model->refreshData();
		return true;
	});
}

void ThreadView::openContextMenu(QPoint pos)
{
	if (!m_ui.threadList->selectionModel()->hasSelection())
		return;

	QMenu* menu = new QMenu(m_ui.threadList);
	menu->setAttribute(Qt::WA_DeleteOnClose);

	QAction* copy = menu->addAction(tr("Copy"));
	connect(copy, &QAction::triggered, [this]() {
		const QItemSelectionModel* selection_model = m_ui.threadList->selectionModel();
		if (!selection_model->hasSelection())
			return;

		auto real_index = m_proxy_model->mapToSource(selection_model->currentIndex());
		QGuiApplication::clipboard()->setText(m_model->data(real_index).toString());
	});

	menu->addSeparator();

	QAction* copy_all_as_csv = menu->addAction(tr("Copy all as CSV"));
	connect(copy_all_as_csv, &QAction::triggered, [this]() {
		QGuiApplication::clipboard()->setText(QtUtils::AbstractItemModelToCSV(m_ui.threadList->model()));
	});

	menu->popup(m_ui.threadList->viewport()->mapToGlobal(pos));
}

void ThreadView::onDoubleClick(const QModelIndex& index)
{
	auto real_index = m_proxy_model->mapToSource(index);
	switch (index.column())
	{
		case ThreadModel::ThreadColumns::ENTRY:
		{
			goToInDisassembler(m_model->data(real_index, Qt::UserRole).toUInt(), true);
			break;
		}
		default: // Default to PC
		{
			QModelIndex pc_index = m_model->index(real_index.row(), ThreadModel::ThreadColumns::PC);
			goToInDisassembler(m_model->data(pc_index, Qt::UserRole).toUInt(), true);
			break;
		}
	}
}
