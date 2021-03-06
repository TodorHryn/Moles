#include "gameview.h"
#include "ui_gameview.h"
#include "teaminfowidget.h"
#include "chooseweapondialog.h"
#include "gameoverdialog.h"
#include "mainwindow.h"
#include <QPainter>
#include <QBrush>
#include <QPainterPath>
#include <QKeyEvent>
#include <QtDebug>
#include <QPalette>

GameView::GameView(QWidget *parent) : QWidget(parent), ui(new Ui::GameView) {
    ui->setupUi(this);

    QFile qss(":/qss/stylesheet.qss");
    qss.open(QFile::ReadOnly);
    stylesheet_ = qss.readAll();
    setStyleSheet(stylesheet_);

    setFocusPolicy(Qt::StrongFocus);
    tickTimer_ = new QTimer(this);
    connect(tickTimer_, SIGNAL(timeout()), this, SLOT(nextTick()));
}

GameView::~GameView() {
    delete ui;
}

void GameView::keyPressEvent(QKeyEvent *event) {
    if (!engine_)
        return;

    Character &c = engine_->currentTeam()->currentCharacter();

    if (event->key() == Qt::Key_D) c.actions().moveRight = true;
    if (event->key() == Qt::Key_A) c.actions().moveLeft = true;
    if (!event->isAutoRepeat() && event->key() == Qt::Key_Space) c.actions().jump = true;

    if (event->key() == Qt::Key_E) {
        c.actions().shoot = true;
    }

    if (event->key() == Qt::Key_R) {
        chooseWeaponDialog = new ChooseWeaponDialog(*engine_);
        chooseWeaponDialog->setStyleSheet(stylesheet_);
        chooseWeaponDialog->exec();
    }
}

void GameView::keyReleaseEvent(QKeyEvent *event) {
    if (!engine_)
        return;

    Character &c = engine_->currentTeam()->currentCharacter();

    if (event->key() == Qt::Key_D) c.actions().moveRight = false;
    if (event->key() == Qt::Key_A) c.actions().moveLeft = false;
    if (event->key() == Qt::Key_Space) c.actions().jump = false;
}

void GameView::mouseMoveEvent(QMouseEvent *event) {
    if (!engine_)
        return;

    Terrain &terrain = engine_->terrain();
    int tileSize = std::min(size_.width() / terrain.size().width(), size_.height() / terrain.size().height());
    QPoint shift((size_.width() - terrain.size().width() * tileSize) / 2, size_.height() - terrain.size().height() * tileSize);

    Character &ch = engine_->currentTeam()->currentCharacter();
    QPoint chPos((ch.position().x() - ch.size().width() / 2) * tileSize, (ch.position().y() - ch.size().height() / 2) * tileSize);
    chPos += shift;

    QVector2D dv(event->pos() - chPos);
    engine_->currentTeam()->currentCharacter().actions().weapon->angle_ = atan2(dv.y(), dv.x());
}

void GameView::resizeEvent(QResizeEvent *event) {
    terrainCache = QImage();
}

void GameView::rebuildUi() {
    for (TeamInfoWidget *widget : teamInfoWidgets) {
        ui->teamsInfoFrame->removeWidget(widget);
        widget->deleteLater();
    }
    teamInfoWidgets.clear();

    if (engine_) {
        for (auto team : engine_->teams()) {
            TeamInfoWidget *widget = new TeamInfoWidget(team);
            ui->teamsInfoFrame->addWidget(widget);

            teamInfoWidgets.push_back(widget);
        }
    }
}

void GameView::nextTick() {
    if (!engine_)
        return;

    if (engine_->terrain().changed()) {
        terrainCache = QImage();
        engine_->terrain().setChanged(false);
    }

    engine_->tick();
    update();

    QString teamAlive = "";
    int teamsAlive = 0;

    for (auto team : engine_->teams()) {
        if (team->isAlive()) {
            teamAlive = team->name();
            teamsAlive++;
        }
    }

    if (teamAlive == 1) {
        GameOverDialog *dlg = new GameOverDialog(teamAlive);
        dlg->setStyleSheet(stylesheet_);
        dlg->exec();
        dlg->deleteLater();

        MainWindow::get(this)->openMainMenu();
    }
}

void GameView::setGameEngine(std::shared_ptr<GameEngine> gameEngine) {
    engine_ = gameEngine;

    if (engine_) {
        rebuildUi();

        tickTimer_->stop();
        tickTimer_->start(1000.0 / 60);
    }
    else
        tickTimer_->stop();
}

std::shared_ptr<GameEngine> GameView::gameEngine() {
    return engine_;
}

void GameView::paintEvent(QPaintEvent *event) {
    if (!engine_)
        return;

    if (framesCount == 0) {
        fpsCounterStart.start();
    }

    if (fpsCounterStart.elapsed() > 1000) {
        framesCount = 0;
        fpsCounterStart.restart();
    }

    QPainter painter(this);
    size_ = painter.window().size();

    Terrain &terrain = engine_->terrain();
    int tileSize = std::min(size_.width() / terrain.size().width(), size_.height() / terrain.size().height());
    QPoint shift((size_.width() - terrain.size().width() * tileSize) / 2, size_.height() - terrain.size().height() * tileSize);

    painter.save();
    painter.fillRect(0, 0, painter.device()->width(), painter.device()->height(), QColor::fromRgb(128, 217, 255));
    painter.translate(shift);

    if (terrainCache.isNull()) {
        terrainCache = QImage(tileSize * terrain.size(), QImage::Format_ARGB32_Premultiplied);
        QPainter terrainPainter(&terrainCache);
        drawTerrain(terrainPainter, tileSize);
    }

    painter.drawImage(0, 0, terrainCache);
    std::shared_ptr<Weapon> weapon = engine_->currentTeam()->currentCharacter().actions().weapon;
    if (weapon) {
        QPen pen(Qt::DotLine);
        pen.setWidth(2);
        painter.save();
        painter.setPen(pen);

        Character &ch = engine_->currentTeam()->currentCharacter();
        QPointF chPos(ch.position().toPointF() * tileSize);
        painter.drawLine(chPos, chPos + tileSize * 10 * QPointF(cos(weapon->angle_), sin(weapon->angle_)));

        painter.restore();
    }
    drawCharacters(painter, tileSize);
    painter.restore();

    drawTeamsInfo(painter);

    framesCount++;
    painter.drawText(10, 10, QString::number(1000 * (double) framesCount / fpsCounterStart.elapsed()));
}

void GameView::drawTeamsInfo(QPainter &painter) {
    ui->currentTeam->setText(engine_->currentTeam()->name());
    ui->currentWeapon->setText(engine_->currentTeam()->currentCharacter().actions().weapon->name());
}

void GameView::drawCharacters(QPainter &painter, int tileSize) {
    painter.save();

    QFont font("Verdana", 12);
    painter.setFont(font);

    for (auto team : engine_->teams()) {
        painter.setBrush(QBrush(team->color(), Qt::SolidPattern));

        for (Character &ch : team->characters()) {
            if (ch.isAlive()) {
                painter.setPen(Qt::black);
                painter.drawRect((ch.position().x() - ch.size().width() / 2) * tileSize,
                                (ch.position().y() - ch.size().height() / 2) * tileSize,
                                 ch.size().width() * tileSize,
                                ch.size().height() * tileSize);

                if (ch.actions().damagedCooldown > 0) {
                    painter.setPen(Qt::red);
                    painter.drawText((ch.position().toPointF() + QPointF(1, 1)) * tileSize, QString::number(ch.actions().damaged));
                }
            }
        }
    }

    painter.restore();
}

void GameView::drawTerrain(QPainter &painter, int tileSize) {
    Terrain &terrain = engine_->terrain();

    for (int x = 0; x < terrain.size().width(); ++x) {
        for (int y = 0; y < terrain.size().height(); ++y) {
            QRect rec(QPoint(x, y) * tileSize, QSize(tileSize, tileSize));
            painter.fillRect(rec, terrain.tiles()[x][y].color());
        }
    }
}

void GameView::on_pushButton_clicked() {
    if (!engine_)
        return;

    engine_->nextTurn();
}
