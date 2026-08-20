QT += widgets
QT += core
QT += multimedia

SOURCES += main.cpp

# The original code includes '#include "main.moc"' at the bottom.
# This line tells qmake to handle the Q_OBJECT usage within main.cpp.
HEADERS += main.moc

CONFIG += c++17
