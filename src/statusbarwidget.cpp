/*
 * This file is part of the https://github.com/QQxiaoming/quardCRT.git
 * project.
 *
 * Copyright (C) 2024 Quard <2014500726@smail.xtu.edu.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QMenu>

#include "globalsetting.h"
#include "qfonticon.h"
#include "statusbarwidget.h"
#include "ui_statusbarwidget.h"

StatusBarWidget::StatusBarWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StatusBarWidget) {
    ui->setupUi(this);
    setMouseTracking(true);

    statusBarNotifiction = new StatusBarToolButton(this);
    ui->horizontalLayout->addWidget(statusBarNotifiction);
    statusBarNotifiction->setIcon(QFontIcon::icon(QChar(0xf0a2)));
    statusBarNotifiction->setPopupMode(QToolButton::InstantPopup);
    statusBarNotifiction->setAutoRaise(true);
}

StatusBarWidget::~StatusBarWidget() {
    delete ui;
}

void StatusBarWidget::setNotifiction(bool enable) {
    if(enable) {
        statusBarNotifiction->setIcon(QFontIcon::icon(QChar(0xf0f3)));
    } else {
        statusBarNotifiction->setIcon(QFontIcon::icon(QChar(0xf0a2)));
    }
}

void StatusBarWidget::contextMenuEvent(QContextMenuEvent *event) {
    Q_UNUSED(event);
}
