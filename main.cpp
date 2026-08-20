#include <QApplication>
#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>
#include <QTimer>
#include <vector>
#include <cstdlib>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QPainter>
#include <QIcon>
#include <QPixmap>
#include <QGraphicsProxyWidget>
#include <QKeyEvent>
#include <QLineEdit>
#include <QIntValidator>
#include <QtGlobal>
#include <ctime>
#include <QPainterPath>

// --- NEW INCLUDES ---
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QUrl>
#include <QLinearGradient>
#include <QGraphicsTextItem>
#include <QImage> // Required for the buffer fix

// --- CONSTANT FOR GRID SIZE ---
const int GRID_SIZE = 5;

// Constants for the game
const int SCENE_WIDTH = 80 * GRID_SIZE; // 400
const int SCENE_HEIGHT = 140 * GRID_SIZE; // 700
const int PLAYER_CAR_WIDTH = 10 * GRID_SIZE; // 50
const int PLAYER_CAR_HEIGHT = 16 * GRID_SIZE; // 80
const int OBSTACLE_CAR_WIDTH = 10 * GRID_SIZE; // 50
const int OBSTACLE_CAR_HEIGHT = 16 * GRID_SIZE; // 80
const int LANE_WIDTH = 20 * GRID_SIZE; // 100
const int NUM_LANES = 3;
const int VICTORY_SCORE = 50;

// --- SPEEDS ---
const int DOG_CROSSING_SPEED = 4;
const int CAR_EXTRA_SPEED = 6;

// --- ENUMS ---
enum SequenceState { STATE_CARS, STATE_DOGS };
enum ObstacleType { TYPE_CAR, TYPE_DOG, TYPE_LOG };

// --- CUSTOM ITEM FOR NIGHT MODE HEADLIGHTS (BUFFERED FIX) ---
class NightOverlayItem : public QGraphicsRectItem {
public:
    NightOverlayItem(qreal w, qreal h) : QGraphicsRectItem(0, 0, w, h) {
        carPos = QPointF(w/2, h - 100);
        carWidth = 50;
        // Create an off-screen image buffer.
        // ARGB32_Premultiplied is the fastest format for drawing.
        overlayBuffer = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    }

    void setCarPosition(QPointF pos, qreal width) {
        carPos = pos;
        carWidth = width;
        update(); // Triggers a repaint
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        Q_UNUSED(option); Q_UNUSED(widget);

        // --- STEP 1: FILL BUFFER WITH SOLID BLACK ---
        overlayBuffer.fill(QColor(0, 0, 0, 255));

        // Create a painter to draw onto our off-screen buffer
        QPainter bufferPainter(&overlayBuffer);
        bufferPainter.setRenderHint(QPainter::Antialiasing, true);

        // --- STEP 2: CUT THE HOLE (HEADLIGHT BEAM) ---
        // Use DestinationOut.
        // Source Alpha 255 -> Removes Alpha 255 from Dest (Makes Transparent).
        // Source Alpha 0   -> Removes Alpha 0 (Keeps Black).
        bufferPainter.setCompositionMode(QPainter::CompositionMode_DestinationOut);

        // Define the beam shape (Cone)
        QPainterPath beamPath;
        qreal startX = carPos.x() + carWidth / 2;
        qreal startY = carPos.y() + 20; // Start slightly down the hood

        beamPath.moveTo(startX - 10, startY);
        beamPath.lineTo(startX - 350, -100); // Wide Left spread off-screen
        beamPath.lineTo(startX + 350, -100); // Wide Right spread off-screen
        beamPath.lineTo(startX + 10, startY);
        beamPath.closeSubpath();

        // Gradient for the cut
        QLinearGradient cutGradient(startX, startY, startX, 0);
        cutGradient.setColorAt(0.0, QColor(0, 0, 0, 255)); // 100% Transparent at car (Clear view of road)
        cutGradient.setColorAt(0.7, QColor(0, 0, 0, 220)); // Mostly transparent mid-way
        cutGradient.setColorAt(1.0, QColor(0, 0, 0, 0));   // Solid black at distance

        bufferPainter.setBrush(cutGradient);
        bufferPainter.setPen(Qt::NoPen);
        bufferPainter.drawPath(beamPath);

        // --- STEP 3: ADD THE TINT (GLOW) ---
        // Switch back to SourceOver to add color ON TOP of the transparent hole
        bufferPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        QRadialGradient glowGradient(QPointF(startX, startY), 600);
        glowGradient.setColorAt(0.0, QColor(255, 255, 200, 50)); // Faint yellow tint
        glowGradient.setColorAt(1.0, QColor(255, 255, 200, 0));  // Fade out

        bufferPainter.setBrush(glowGradient);
        bufferPainter.drawPath(beamPath);

        bufferPainter.end(); // Finish painting to buffer

        // --- STEP 4: DRAW THE BUFFER TO SCREEN ---
        painter->drawImage(0, 0, overlayBuffer);
    }

private:
    QPointF carPos;
    qreal carWidth;
    QImage overlayBuffer; // The off-screen canvas
};

// --- CAR ITEM CLASS ---
class CarItem : public QGraphicsRectItem {
public:
    CarItem(qreal x, qreal y, qreal width, qreal height, const QColor& color)
        : QGraphicsRectItem(x, y, width, height), carColor(color) {}

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        Q_UNUSED(option); Q_UNUSED(widget);
        QRectF r = rect();
        painter->setBrush(carColor);
        painter->setPen(QPen(Qt::black, 1));
        painter->drawRect(r);

        qreal cabinWidth = r.width() * 0.7;
        qreal cabinHeight = r.height() * 0.5;
        qreal cabinX = r.x() + qRound((r.width() - cabinWidth) / 2.0);
        qreal cabinY = r.y() + qRound(r.height() * 0.1);
        painter->setBrush(QColor(20, 20, 20));
        painter->setPen(Qt::NoPen);
        painter->drawRect(cabinX, cabinY, cabinWidth, cabinHeight);

        qreal headlightWidth = r.width() * 0.25;
        qreal headlightHeight = r.height() * 0.1;
        qreal headlightY = r.y() + qRound(r.height() * 0.05);
        painter->setBrush(Qt::yellow);
        painter->drawRect(r.x() + qRound(r.width() * 0.1), headlightY, headlightWidth, headlightHeight);
        painter->drawRect(r.x() + qRound(r.width() * 0.65), headlightY, headlightWidth, headlightHeight);

        qreal taillightY = r.y() + qRound(r.height() * 0.85);
        painter->setBrush(Qt::red);
        painter->drawRect(r.x() + qRound(r.width() * 0.1), taillightY, headlightWidth, headlightHeight);
        painter->drawRect(r.x() + qRound(r.width() * 0.65), taillightY, headlightWidth, headlightHeight);
    }
private:
    QColor carColor;
};

// --- OBSTACLE CLASS ---
class ObstacleObject : public CarItem {
public:
    ObstacleObject(qreal x, qreal y, qreal width, qreal height, const QColor& color, ObstacleType type = TYPE_CAR)
        : CarItem(x, y, width, height, color), isLaneChanger(false), targetLane(0), currentLane(0), obstacleType(type), isHit(false)
    {
        if (obstacleType == TYPE_DOG) { setRect(x, y, 3 * GRID_SIZE, 3 * GRID_SIZE); setPen(Qt::NoPen); }
        else if (obstacleType == TYPE_LOG) { setRect(x, y, 2 * LANE_WIDTH, 4 * GRID_SIZE); setPen(Qt::NoPen); }
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override {
        QRectF r = rect();
        painter->setRenderHint(QPainter::Antialiasing, false);

        if (obstacleType == TYPE_CAR) {
            CarItem::paint(painter, option, widget);
        }
        else if (obstacleType == TYPE_LOG) {
            QColor woodLight = QColor("#8B4513"); QColor woodDark = QColor("#654321");
            painter->setBrush(woodLight); painter->setPen(QPen(Qt::black, 1)); painter->drawRect(r);
            painter->setPen(Qt::NoPen); painter->setBrush(woodDark);
            qreal h = r.height();
            painter->drawRect(r.x(), r.y() + h*0.2, r.width(), h*0.1);
            painter->drawRect(r.x(), r.y() + h*0.6, r.width(), h*0.1);
            painter->setBrush(QColor(60, 30, 0));
            painter->drawEllipse(r.x() + r.width()*0.2, r.y() + h*0.3, h*0.4, h*0.4);
            painter->drawEllipse(r.x() + r.width()*0.7, r.y() + h*0.2, h*0.5, h*0.5);
        }
        else if (obstacleType == TYPE_DOG) {
            if (isHit) {
                painter->setBrush(QColor(100, 100, 100, 200)); painter->setPen(QPen(Qt::black, 1));
                painter->drawRect(r.adjusted(0, r.height() * 0.3, 0, -r.height() * 0.3));
                painter->setPen(QPen(Qt::red, 3));
                painter->drawLine(r.x(), r.y(), r.right(), r.bottom());
                painter->drawLine(r.right(), r.y(), r.x(), r.bottom());
            } else {
                QColor b1 = QColor("#f0f0f0"); QColor b2 = QColor("#d0c0b0"); QColor col = QColor("#e74c3c");
                painter->setPen(Qt::NoPen); qreal gs = GRID_SIZE;
                painter->setBrush(b1); painter->drawRect(r.x(), r.y()+gs, 2*gs, 2*gs);
                painter->setBrush(b2); painter->drawRect(r.x()+2*gs, r.y()+gs, gs, gs);
                painter->setBrush(b1); painter->drawRect(r.x()+2*gs, r.y(), gs, gs);
                painter->setBrush(b2); painter->drawRect(r.x()+2*gs+(gs/2), r.y()-gs/2, gs/2, gs/2);
                painter->setBrush(b1); painter->drawRect(r.x()-gs, r.y()+gs, gs, gs);
                painter->setBrush(b2); painter->drawRect(r.x()+gs/2, r.y()+2*gs, gs/2, gs/2);
                painter->drawRect(r.x()+2*gs-gs/2, r.y()+2*gs, gs/2, gs/2);
                painter->setBrush(col); painter->drawRect(r.x()+2*gs, r.y()+gs/2, gs/2, gs/2);
                painter->setBrush(Qt::black); painter->drawRect(r.x()+2*gs+(gs/2), r.y()+(gs/2)-(gs/4), gs/4, gs/4);
            }
        }
    }
    bool isLaneChanger; int targetLane; int currentLane; ObstacleType obstacleType; bool isHit;
};


class GameWindow : public QMainWindow {
    Q_OBJECT

public:
    GameWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("Car Lane Racer (Grid)");
        setFixedSize(SCENE_WIDTH + 250, SCENE_HEIGHT + 50);
        playerCarColor = Qt::cyan;
        setFocusPolicy(Qt::StrongFocus);
        mainStackedWidget = new QStackedWidget(this);
        setCentralWidget(mainStackedWidget);
        setupMainMenu();

        gameScene = new QGraphicsScene(this);
        gameView = new QGraphicsView(gameScene);
        gameView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        gameView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        gameView->setRenderHint(QPainter::Antialiasing, false);
        gameView->setRenderHint(QPainter::SmoothPixmapTransform, false);

        gameLoopTimer = new QTimer(this);
        connect(gameLoopTimer, &QTimer::timeout, this, &GameWindow::gameLoop);
        obstacleSpawnTimer = new QTimer(this);
        connect(obstacleSpawnTimer, &QTimer::timeout, this, &GameWindow::spawnObstacle);

        finishLine = nullptr; finishLineSpawned = false; nightOverlay = nullptr; nightModeTextItem = nullptr; crashDebris.clear();

        carSoundPlayer = new QMediaPlayer(this); audioOutput = new QAudioOutput(this); carSoundPlayer->setAudioOutput(audioOutput);
        carSoundPlayer->setSource(QUrl::fromLocalFile("car_sound.wav"));
        connect(carSoundPlayer, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) { if (status == QMediaPlayer::EndOfMedia) carSoundPlayer->play(); });
        audioOutput->setVolume(0.3);
        crashSoundPlayer = new QMediaPlayer(this); crashAudioOutput = new QAudioOutput(this); crashSoundPlayer->setAudioOutput(crashAudioOutput);
        crashSoundPlayer->setSource(QUrl::fromLocalFile("crash.wav")); crashAudioOutput->setVolume(0.5);
        victorySoundPlayer = new QMediaPlayer(this); victoryAudioOutput = new QAudioOutput(this); victorySoundPlayer->setAudioOutput(victoryAudioOutput);
        victorySoundPlayer->setSource(QUrl::fromLocalFile("victory.wav")); victoryAudioOutput->setVolume(0.5);
    }

protected:
    void keyPressEvent(QKeyEvent *event) override {
        if (mainStackedWidget->currentWidget() == gameContainerWidget && gameLoopTimer->isActive()) {
            if (event->key() == Qt::Key_Left) movePlayerLeft();
            else if (event->key() == Qt::Key_Right) movePlayerRight();
        }
        QMainWindow::keyPressEvent(event);
    }

private slots:
    void startGame(int level) {
        currentLevel = level;
        obstacleSpeed = 12;
        int spawnRate = 500;
        score = 0;
        finishLineSpawned = false;

        if (currentLevel >= 2) {
            obstacleSequenceState = STATE_CARS; currentSequenceLength = 5; obstaclesRemainingInState = currentSequenceLength;
        } else {
            obstacleSequenceState = STATE_CARS; currentSequenceLength = -1; obstaclesRemainingInState = -1;
        }

        gameScene->clear();
        playerCar = nullptr; finishLine = nullptr; nightOverlay = nullptr; nightModeTextItem = nullptr;
        obstacles.clear(); crashDebris.clear();
        setupGameUI();

        int laneAreaWidth = NUM_LANES * LANE_WIDTH;
        int xOffset = (SCENE_WIDTH - laneAreaWidth) / 2;
        for (int i = 1; i < NUM_LANES; ++i) gameScene->addLine(xOffset + i * LANE_WIDTH, 0, xOffset + i * LANE_WIDTH, SCENE_HEIGHT, QPen(Qt::white, 2, Qt::DashLine));
        QPen gridPen(QColor(0, 0, 0, 100), 1, Qt::DotLine);
        for (int x = xOffset; x <= xOffset + laneAreaWidth; x += GRID_SIZE) if (x % LANE_WIDTH != 0) { QGraphicsLineItem* l = gameScene->addLine(x, 0, x, SCENE_HEIGHT, gridPen); l->setZValue(-1); }
        for (int y = 0; y <= SCENE_HEIGHT; y += GRID_SIZE) { QGraphicsLineItem* l = gameScene->addLine(xOffset, y, xOffset + laneAreaWidth, y, gridPen); l->setZValue(-1); }

        // --- 1. CREATE PLAYER CAR (Z-Value = 6) ---
        // Must be higher than the NightOverlay (Z=5) so it appears on top of the darkness
        playerCar = new CarItem(0, 0, PLAYER_CAR_WIDTH, PLAYER_CAR_HEIGHT, playerCarColor);
        playerCar->setZValue(6);
        gameScene->addItem(playerCar);

        currentLane = 1;
        int initialY = SCENE_HEIGHT - PLAYER_CAR_HEIGHT - 4 * GRID_SIZE;
        initialY = (initialY / GRID_SIZE) * GRID_SIZE;
        playerCar->setPos(0, initialY);
        updatePlayerPosition();

        // --- 2. NIGHT MODE SETUP (Z-Value = 5) ---
        if (currentLevel == 3) {
            nightOverlay = new NightOverlayItem(SCENE_WIDTH, SCENE_HEIGHT);
            nightOverlay->setZValue(5); // Below car, above grid/obstacles
            nightOverlay->setCarPosition(playerCar->pos(), PLAYER_CAR_WIDTH);
            gameScene->addItem(nightOverlay);

            nightModeTextItem = new QGraphicsTextItem("Night Mode");
            QFont font("Arial", 24, QFont::Bold);
            nightModeTextItem->setFont(font);
            nightModeTextItem->setDefaultTextColor(QColor(200, 200, 200, 255));
            qreal textWidth = nightModeTextItem->boundingRect().width();
            nightModeTextItem->setPos((SCENE_WIDTH - textWidth) / 2.0, 50);
            nightModeTextItem->setZValue(7); // Text on top of everything
            gameScene->addItem(nightModeTextItem);
        }

        gameLoopTimer->start(16);
        obstacleSpawnTimer->start(spawnRate);
        carSoundPlayer->play();
    }

    void movePlayerLeft() {
        if (currentLane > 0) {
            currentLane--;
            updatePlayerPosition();
            if (nightOverlay) nightOverlay->setCarPosition(playerCar->pos(), PLAYER_CAR_WIDTH);
        }
    }

    void movePlayerRight() {
        if (currentLane < NUM_LANES - 1) {
            currentLane++;
            updatePlayerPosition();
            if (nightOverlay) nightOverlay->setCarPosition(playerCar->pos(), PLAYER_CAR_WIDTH);
        }
    }

    void gameLoop() {
        if (finishLineSpawned && finishLine) {
            finishLine->setY(finishLine->y() + obstacleSpeed);
            if (finishLine->collidesWithItem(playerCar)) { score = VICTORY_SCORE; updateScoreLabel(); endGame(true); return; }
            if (finishLine->y() > SCENE_HEIGHT) { endGame(false); return; }
        }

        for (size_t i = 0; i < obstacles.size(); ) {
            ObstacleObject* obstacle = obstacles[i];
            if (obstacle->obstacleType == TYPE_DOG && !obstacle->isHit) obstacle->setX(obstacle->x() + DOG_CROSSING_SPEED);
            if (obstacle->obstacleType == TYPE_CAR && obstacle->isLaneChanger) {
                if (rand() % 60 == 0) {
                    int direction = (rand() % 2) * 2 - 1;
                    int newLane = obstacle->currentLane + direction;
                    if (newLane >= 0 && newLane < NUM_LANES) obstacle->targetLane = newLane;
                }
                if (obstacle->currentLane != obstacle->targetLane) {
                    int laneAreaWidth = NUM_LANES * LANE_WIDTH;
                    int xOffset = (SCENE_WIDTH - laneAreaWidth) / 2;
                    int targetX = xOffset + (obstacle->targetLane * LANE_WIDTH) + (LANE_WIDTH / 2) - (OBSTACLE_CAR_WIDTH / 2);
                    if (obstacle->x() < targetX) obstacle->setX(qMin(obstacle->x() + 4, (qreal)targetX));
                    else if (obstacle->x() > targetX) obstacle->setX(qMax(obstacle->x() - 4, (qreal)targetX));
                    if (qAbs(obstacle->x() - targetX) < 1) obstacle->currentLane = obstacle->targetLane;
                }
            }
            qreal currentSpeed = obstacleSpeed;
            if (obstacle->obstacleType == TYPE_CAR) currentSpeed += CAR_EXTRA_SPEED;

            obstacle->setY(obstacle->y() + currentSpeed);

            if (!obstacle->isHit && obstacle->collidesWithItem(playerCar)) {
                if (obstacle->obstacleType == TYPE_DOG) { obstacle->isHit = true; endGame(false, true); }
                else { endGame(false); }
                return;
            }
            bool shouldRemove = false;
            if (obstacle->y() > SCENE_HEIGHT) shouldRemove = true;
            if (obstacle->obstacleType == TYPE_DOG && obstacle->x() > SCENE_WIDTH) shouldRemove = true;

            if (shouldRemove) {
                if (obstacle->obstacleType == TYPE_CAR || obstacle->obstacleType == TYPE_LOG) { score++; updateScoreLabel(); }
                gameScene->removeItem(obstacle); delete obstacle; obstacles.erase(obstacles.begin() + i);
            } else { i++; }
        }
    }

    void spawnObstacle() {
        if (score >= VICTORY_SCORE - 1 && !finishLineSpawned) {
            int laneAreaWidth = NUM_LANES * LANE_WIDTH;
            int xOffset = (SCENE_WIDTH - laneAreaWidth) / 2;
            finishLine = new QGraphicsRectItem(0, 0, laneAreaWidth, 4 * GRID_SIZE);
            QPixmap checker(2 * GRID_SIZE, 2 * GRID_SIZE); checker.fill(Qt::transparent);
            QPainter p(&checker); p.setRenderHint(QPainter::Antialiasing, false);
            p.fillRect(0, 0, GRID_SIZE, GRID_SIZE, Qt::black); p.fillRect(GRID_SIZE, GRID_SIZE, GRID_SIZE, GRID_SIZE, Qt::black);
            p.fillRect(0, GRID_SIZE, GRID_SIZE, GRID_SIZE, Qt::white); p.fillRect(GRID_SIZE, 0, GRID_SIZE, GRID_SIZE, Qt::white); p.end();
            finishLine->setBrush(QBrush(checker)); finishLine->setPen(Qt::NoPen); finishLine->setPos(xOffset, -4 * GRID_SIZE);
            finishLine->setZValue(1); gameScene->addItem(finishLine); finishLineSpawned = true; return;
        }
        if(finishLineSpawned) return;

        ObstacleType typeToSpawn = TYPE_CAR;
        if (currentLevel >= 2) {
            if (obstaclesRemainingInState <= 0) {
                if (obstacleSequenceState == STATE_CARS) { obstacleSequenceState = STATE_DOGS; currentSequenceLength = 1; obstaclesRemainingInState = currentSequenceLength; }
                else { obstacleSequenceState = STATE_CARS; currentSequenceLength = 5; obstaclesRemainingInState = currentSequenceLength; }
            }
            if (obstacleSequenceState == STATE_DOGS) typeToSpawn = TYPE_DOG;
            else { if (rand() % 100 < 30) typeToSpawn = TYPE_LOG; else typeToSpawn = TYPE_CAR; }
            obstaclesRemainingInState--;
        }

        int laneAreaWidth = NUM_LANES * LANE_WIDTH;
        int xOffset = (SCENE_WIDTH - laneAreaWidth) / 2;
        ObstacleObject* obstacle = nullptr;

        if (typeToSpawn == TYPE_CAR) {
            int lane = rand() % NUM_LANES;
            for (ObstacleObject* obs : obstacles) {
                if (obs->obstacleType == TYPE_LOG) {
                    int logLaneStart = obs->currentLane;
                    if (lane == logLaneStart || lane == logLaneStart + 1) { obstaclesRemainingInState++; return; }
                }
                else if (obs->obstacleType == TYPE_DOG) {
                    qreal carSpawnY = qRound((qreal)-OBSTACLE_CAR_HEIGHT / GRID_SIZE) * GRID_SIZE;
                    qreal dogY = obs->y();
                    if (dogY > carSpawnY) {
                        qreal distanceToCatchUp = dogY - carSpawnY;
                        qreal relativeSpeed = CAR_EXTRA_SPEED;
                        qreal framesUntilImpact = distanceToCatchUp / relativeSpeed;
                        qreal dogXAtImpact = obs->x() + (DOG_CROSSING_SPEED * framesUntilImpact);
                        qreal dogWidth = obs->rect().width();
                        qreal carLeft = xOffset + (lane * LANE_WIDTH) + (LANE_WIDTH/2) - (OBSTACLE_CAR_WIDTH/2);
                        qreal carRight = carLeft + OBSTACLE_CAR_WIDTH;
                        if (dogXAtImpact < carRight && dogXAtImpact + dogWidth > carLeft) { obstaclesRemainingInState++; return; }
                    }
                }
            }
            QColor colors[] = {QColor("#e74c3c"), QColor("#f1c40f"), QColor("#2ecc71"), QColor("#9b59b6"), QColor("#3498db")};
            QColor randomColor = colors[rand() % 5];
            int laneCenter = xOffset + (lane * LANE_WIDTH) + (LANE_WIDTH / 2);
            obstacle = new ObstacleObject(0, 0, OBSTACLE_CAR_WIDTH, OBSTACLE_CAR_HEIGHT, randomColor, TYPE_CAR);
            obstacle->currentLane = lane; obstacle->targetLane = lane;
            if (currentLevel >= 2 && (rand() % 3 == 0)) { obstacle->isLaneChanger = true; obstacle->setBrush(Qt::white); }
            qreal spawnY = qRound((qreal)-OBSTACLE_CAR_HEIGHT / GRID_SIZE) * GRID_SIZE;
            obstacle->setPos(laneCenter - OBSTACLE_CAR_WIDTH / 2, spawnY);

        } else if (typeToSpawn == TYPE_LOG) {
            int startLane = rand() % (NUM_LANES - 1);
            int logX = xOffset + (startLane * LANE_WIDTH);
            obstacle = new ObstacleObject(0, 0, 0, 0, Qt::transparent, TYPE_LOG);
            obstacle->currentLane = startLane;
            qreal spawnY = qRound(-4.0 * GRID_SIZE);
            obstacle->setPos(logX, spawnY);
        }
        else if (typeToSpawn == TYPE_DOG) {
            qreal dogWidth = 3 * GRID_SIZE; qreal dogHeight = 3 * GRID_SIZE;
            int startX = xOffset - dogWidth;
            obstacle = new ObstacleObject(0, 0, dogWidth, dogHeight, QColor("#a0522d"), TYPE_DOG);
            qreal maxDogY = SCENE_HEIGHT * 0.3;
            qreal initialDogY = -dogHeight + (rand() % (int)(maxDogY / GRID_SIZE)) * GRID_SIZE;
            obstacle->setPos(startX, initialDogY);
        }

        if (obstacle) { obstacle->setZValue(1); gameScene->addItem(obstacle); obstacles.push_back(obstacle); }
    }

    void returnToMenu() {
        gameLoopTimer->stop(); obstacleSpawnTimer->stop(); carSoundPlayer->stop(); gameScene->clear();
        playerCar = nullptr; finishLine = nullptr; nightOverlay = nullptr; nightModeTextItem = nullptr;
        obstacles.clear(); crashDebris.clear(); mainStackedWidget->setCurrentWidget(mainMenuWidget);
    }

private:
    void setupMainMenu() {
        mainMenuWidget = new QWidget(); QVBoxLayout* menuLayout = new QVBoxLayout(mainMenuWidget);
        menuLayout->setAlignment(Qt::AlignCenter); menuLayout->setSpacing(20);
        QLabel* titleLabel = new QLabel("Car Lane Racer (Grid)");
        QFont titleFont = titleLabel->font(); titleFont.setPointSize(24); titleFont.setBold(true); titleLabel->setFont(titleFont);
        menuLayout->addWidget(titleLabel, 0, Qt::AlignCenter);
        QLabel* carChoiceLabel = new QLabel("Choose Your Car"); QFont choiceFont = carChoiceLabel->font(); choiceFont.setPointSize(12);
        carChoiceLabel->setFont(choiceFont); menuLayout->addWidget(carChoiceLabel, 0, Qt::AlignCenter);
        QHBoxLayout* carSelectionLayout = new QHBoxLayout(); carSelectionLayout->setSpacing(15); carSelectionLayout->setAlignment(Qt::AlignCenter);
        std::vector<QColor> carColors = {Qt::cyan, Qt::magenta, QColor("#f1c40f"), QColor("#2ecc71")}; playerCarColor = carColors[0];
        carChoiceButtons.clear();
        for (const QColor& color : carColors) {
            QPushButton* carButton = new QPushButton(); carButton->setFixedSize(60, 90);
            QPixmap pixmap(PLAYER_CAR_WIDTH, PLAYER_CAR_HEIGHT); pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap); painter.setRenderHint(QPainter::Antialiasing, false);
            CarItem carDrawer(0, 0, PLAYER_CAR_WIDTH, PLAYER_CAR_HEIGHT, color);
            painter.translate(-carDrawer.rect().x(), -carDrawer.rect().y()); carDrawer.paint(&painter, nullptr, nullptr);
            carButton->setIcon(QIcon(pixmap)); carButton->setIconSize(QSize(50, 80));
            carChoiceButtons.push_back(carButton);
            connect(carButton, &QPushButton::clicked, this, [this, color, carButton]() { this->playerCarColor = color; updateCarSelectionUI(carButton); });
            carSelectionLayout->addWidget(carButton);
        }
        menuLayout->addLayout(carSelectionLayout); updateCarSelectionUI(carChoiceButtons[0]);
        QLabel* instructionLabel = new QLabel("Select a Level"); instructionLabel->setFont(choiceFont); menuLayout->addWidget(instructionLabel, 0, Qt::AlignCenter);
        QHBoxLayout* levelButtonLayout = new QHBoxLayout(); levelButtonLayout->setSpacing(10); levelButtonLayout->setAlignment(Qt::AlignCenter);
        QPushButton* level1Button = new QPushButton("EASY"); level1Button->setFixedSize(100, 50); connect(level1Button, &QPushButton::clicked, this, [this](){ startGame(1); });
        QPushButton* level2Button = new QPushButton("MODERATE"); level2Button->setFixedSize(100, 50); connect(level2Button, &QPushButton::clicked, this, [this](){ startGame(2); });
        QPushButton* level3Button = new QPushButton("HARD"); level3Button->setFixedSize(100, 50); connect(level3Button, &QPushButton::clicked, this, [this](){ startGame(3); });
        levelButtonLayout->addWidget(level1Button); levelButtonLayout->addWidget(level2Button); levelButtonLayout->addWidget(level3Button);
        menuLayout->addLayout(levelButtonLayout); mainStackedWidget->addWidget(mainMenuWidget);
    }

    void updateCarSelectionUI(QPushButton* selectedButton) {
        for (QPushButton* button : carChoiceButtons) {
            QString styleSheet; if (button == selectedButton) styleSheet = "background-color: transparent; border: 4px solid yellow;";
            else styleSheet = "background-color: transparent; border: 2px solid black;"; button->setStyleSheet(styleSheet);
        }
    }

    void setupGameUI() {
        if (mainStackedWidget->count() < 2) {
            gameContainerWidget = new QWidget(); QHBoxLayout *mainLayout = new QHBoxLayout(gameContainerWidget);
            gameScene->setSceneRect(0, 0, SCENE_WIDTH, SCENE_HEIGHT); gameScene->setBackgroundBrush(QBrush(Qt::darkGray));
            mainLayout->addWidget(gameView);
            QVBoxLayout* uiLayout = new QVBoxLayout(); uiLayout->setAlignment(Qt::AlignCenter);
            levelLabel = new QLabel(); scoreLabel = new QLabel();
            QFont labelFont = scoreLabel->font(); labelFont.setPointSize(14); levelLabel->setFont(labelFont); scoreLabel->setFont(labelFont);
            QPushButton* leftButton = new QPushButton("< Left"); QPushButton* rightButton = new QPushButton("Right >");
            leftButton->setFixedSize(80, 50); rightButton->setFixedSize(80, 50);
            connect(leftButton, &QPushButton::clicked, this, &GameWindow::movePlayerLeft); connect(rightButton, &QPushButton::clicked, this, &GameWindow::movePlayerRight);
            uiLayout->addWidget(levelLabel); uiLayout->addSpacing(10); uiLayout->addWidget(scoreLabel); uiLayout->addStretch();
            QHBoxLayout* buttonLayout = new QHBoxLayout(); buttonLayout->addWidget(leftButton); buttonLayout->addWidget(rightButton);
            uiLayout->addLayout(buttonLayout); uiLayout->addStretch(); mainLayout->addLayout(uiLayout);
            mainStackedWidget->addWidget(gameContainerWidget);
        }
        updateScoreLabel(); QString levelName; if (currentLevel == 1) levelName = "Easy"; else if (currentLevel == 2) levelName = "Moderate"; else if (currentLevel == 3) levelName = "Hard";
        levelLabel->setText(QString("Level: %1").arg(levelName)); mainStackedWidget->setCurrentWidget(gameContainerWidget); setFocus();
    }

    void updatePlayerPosition() {
        int laneAreaWidth = NUM_LANES * LANE_WIDTH; int xOffset = (SCENE_WIDTH - laneAreaWidth) / 2;
        int laneCenter = xOffset + (currentLane * LANE_WIDTH) + (LANE_WIDTH / 2); int newX = laneCenter - PLAYER_CAR_WIDTH / 2;
        playerCar->setPos(newX, playerCar->y());
    }

    void updateScoreLabel() { scoreLabel->setText(QString("Score: %1 / %2").arg(score).arg(VICTORY_SCORE)); }

    void endGame(bool victory, bool dogHit = false) {
        gameLoopTimer->stop(); obstacleSpawnTimer->stop(); carSoundPlayer->stop();
        if (victory) { victorySoundPlayer->setPosition(0); victorySoundPlayer->play(); } else if (!dogHit) {
            crashSoundPlayer->setPosition(0); crashSoundPlayer->play();
            qreal carX = playerCar->pos().x(); qreal carY = playerCar->pos().y();
            QGraphicsRectItem* burntCar = new QGraphicsRectItem(playerCar->rect());
            burntCar->setPos(carX, carY); burntCar->setBrush(QColor(30, 30, 30, 200)); burntCar->setPen(Qt::NoPen); burntCar->setZValue(8); gameScene->addItem(burntCar); crashDebris.push_back(burntCar);
            QColor particleColors[] = {QColor("#d35400"), Qt::red, Qt::yellow, QColor(80, 80, 80)};
            for (int i = 0; i < 20; ++i) {
                int pX = carX + (rand() % (int)(PLAYER_CAR_WIDTH * 1.5)) - (PLAYER_CAR_WIDTH * 0.25);
                int pY = carY + (rand() % (int)(PLAYER_CAR_HEIGHT * 1.5)) - (PLAYER_CAR_HEIGHT * 0.25);
                pX = (pX / GRID_SIZE) * GRID_SIZE; pY = (pY / GRID_SIZE) * GRID_SIZE;
                QGraphicsRectItem* particle = new QGraphicsRectItem(0, 0, GRID_SIZE, GRID_SIZE);
                particle->setPos(pX, pY); particle->setBrush(particleColors[rand() % 4]); particle->setPen(Qt::NoPen); particle->setZValue(9); gameScene->addItem(particle); crashDebris.push_back(particle);
            }
        }
        QWidget* messageWidget = new QWidget(); messageWidget->setStyleSheet("background-color: rgba(0, 0, 0, 180); border-radius: 15px;"); messageWidget->setMinimumWidth(350);
        QVBoxLayout* layout = new QVBoxLayout(messageWidget); layout->setAlignment(Qt::AlignCenter); layout->setSpacing(15); layout->setContentsMargins(20, 20, 20, 20);
        QLabel* messageLabel = new QLabel(); QFont font = messageLabel->font(); font.setPointSize(28); font.setBold(true); messageLabel->setFont(font);
        messageLabel->setAlignment(Qt::AlignCenter); messageLabel->setStyleSheet("background-color: transparent; color: white;");
        if (victory) messageLabel->setText("Victory!!!"); else if (dogHit) messageLabel->setText("Game Over (Dog Hit)"); else messageLabel->setText("Try Again");
        QPushButton* menuButton = new QPushButton("Return to Menu"); menuButton->setFixedSize(150, 40);
        menuButton->setStyleSheet("QPushButton { background-color: #3498db; color: white; border: none; border-radius: 5px; } QPushButton:hover { background-color: #2980b9; }");
        layout->addWidget(messageLabel); layout->addWidget(menuButton);
        QGraphicsProxyWidget* proxy = gameScene->addWidget(messageWidget); connect(menuButton, &QPushButton::clicked, this, &GameWindow::returnToMenu);
        qreal boxWidth = messageWidget->sizeHint().width(); qreal boxHeight = messageWidget->sizeHint().height();
        proxy->setPos(qRound(SCENE_WIDTH / 2.0 - boxWidth / 2.0), qRound(SCENE_HEIGHT / 2.0 - boxHeight / 2.0)); proxy->setZValue(10);
    }

    int currentLevel, currentLane, score, obstacleSpeed; QColor playerCarColor; bool finishLineSpawned;
    SequenceState obstacleSequenceState; int obstaclesRemainingInState, currentSequenceLength;
    QStackedWidget* mainStackedWidget; QWidget* mainMenuWidget; QWidget* gameContainerWidget; QGraphicsView* gameView;
    QLabel* scoreLabel; QLabel* levelLabel; std::vector<QPushButton*> carChoiceButtons;
    QGraphicsScene* gameScene; CarItem* playerCar; std::vector<ObstacleObject*> obstacles; QGraphicsRectItem* finishLine;
    NightOverlayItem* nightOverlay; // Changed to custom type
    QGraphicsTextItem* nightModeTextItem; std::vector<QGraphicsRectItem*> crashDebris;
    QTimer* gameLoopTimer; QTimer* obstacleSpawnTimer;
    QMediaPlayer* carSoundPlayer; QAudioOutput* audioOutput; QMediaPlayer* crashSoundPlayer; QAudioOutput* crashAudioOutput; QMediaPlayer* victorySoundPlayer; QAudioOutput* victoryAudioOutput;
};

#include "main.moc"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    srand(time(0));
    GameWindow w;
    w.show();
    return a.exec();
}
