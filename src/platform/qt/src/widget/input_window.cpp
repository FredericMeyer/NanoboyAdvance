/*
 * Copyright (C) 2023 fleroviux
 *
 * Licensed under GPLv3 or any later version.
 * Refer to the included LICENSE file.
 */

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <SDL.h>

#include "widget/input_window.hpp"

InputWindow::InputWindow(
  QApplication* app,
  QWidget* parent,
  std::shared_ptr<QtConfig> config
)   : QDialog(parent)
    , config(config) {
  auto vbox = new QVBoxLayout{this};
  vbox->setSizeConstraint(QLayout::SetFixedSize);
  vbox->addLayout(CreateKeyMapTable());
  vbox->addLayout(CreateGameControllerList());

  app->installEventFilter(this);

  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
  setWindowTitle("Input Config");
}

bool InputWindow::eventFilter(QObject* obj, QEvent* event) {
  if(waiting_for_keyboard && event->type() == QEvent::KeyPress) {
    auto key_event = dynamic_cast<QKeyEvent*>(event);
    auto key = key_event->key();
    auto name = QKeySequence{key_event->key()}.toString();

    active_mapping->keyboard = key;
    active_button->setText(GetKeyboardButtonName(key));
    waiting_for_keyboard = false;
    config->Save();
    return true;
  }

  if(obj == this && event->type() == QEvent::Close) {
    // Cancel the active assignment when the dialog was closed.
    RestoreActiveButtonLabel();
    return true;
  }

  return QObject::eventFilter(obj, event);
}

void InputWindow::BindCurrentKeyToControllerButton(int button) {
  if(waiting_for_controller) {
    active_mapping->controller.button = button;
    active_button->setText(GetControllerButtonName(active_mapping));
    waiting_for_controller = false;
    config->Save();
  }
}

void InputWindow::BindCurrentKeyToControllerAxis(int axis, bool negative) {
  if(waiting_for_controller) {
    active_mapping->controller.axis = axis | (negative ? 0x80 : 0);
    active_button->setText(GetControllerButtonName(active_mapping));
    waiting_for_controller = false;
    config->Save();
  }
}

auto InputWindow::CreateGameControllerList() -> QLayout* {
  auto hbox = new QHBoxLayout{};

  controller_combo_box = new QComboBox();

  auto label = new QLabel{"Game Controller:"};
  hbox->addWidget(label);
  hbox->addWidget(controller_combo_box);

  connect(controller_combo_box, QOverload<int>::of(&QComboBox::activated), [this](int index) {
    config->input.controller_guid = controller_combo_box->itemData(index).toString().toStdString();
    config->Save();

    has_game_controller_choice_changed = true;
  });

  UpdateGameControllerList();
  return hbox;
}

void InputWindow::UpdateGameControllerList() {
  auto joystick_count = SDL_NumJoysticks();

  controller_combo_box->clear();

  controller_combo_box->addItem("(none)", "");
  controller_combo_box->setCurrentIndex(0);

  for(int i = 0; i < joystick_count; i++) {
    auto guid = GetControllerGUIDStringFromIndex(i);

    controller_combo_box->addItem(SDL_JoystickNameForIndex(i), QString::fromStdString(guid));

    if(guid == config->input.controller_guid) {
      controller_combo_box->setCurrentIndex(controller_combo_box->count() - 1);
    }
  }
}

auto InputWindow::CreateKeyMapTable() -> QLayout* {
  auto grid = new QGridLayout{};

  grid->addWidget(new QLabel{tr("Keyboard")}, 0, 1);
  grid->addWidget(new QLabel{tr("Game Controller")}, 0, 2);

  CreateKeyMapEntry(grid, "A", &config->input.gba[int(Key::A)]);
  CreateKeyMapEntry(grid, "B", &config->input.gba[int(Key::B)]);
  CreateKeyMapEntry(grid, "L", &config->input.gba[int(Key::L)]);
  CreateKeyMapEntry(grid, "R", &config->input.gba[int(Key::R)]);
  CreateKeyMapEntry(grid, "Start", &config->input.gba[int(Key::Start)]);
  CreateKeyMapEntry(grid, "Select", &config->input.gba[int(Key::Select)]);
  CreateKeyMapEntry(grid, "Up", &config->input.gba[int(Key::Up)]);
  CreateKeyMapEntry(grid, "Down", &config->input.gba[int(Key::Down)]);
  CreateKeyMapEntry(grid, "Left", &config->input.gba[int(Key::Left)]);
  CreateKeyMapEntry(grid, "Right", &config->input.gba[int(Key::Right)]);
  CreateKeyMapEntry(grid, "Fast Forward", &config->input.fast_forward);
  return grid;
}

void InputWindow::CreateKeyMapEntry(
  QGridLayout* layout,
  const char* label,
  QtConfig::Input::Map* mapping
) {
  auto row = layout->rowCount();

  layout->addWidget(new QLabel{label}, row, 0);

  QPushButton* button_keyboard;
  QPushButton* button_controller;

  {
    button_keyboard = new QPushButton{GetKeyboardButtonName(mapping->keyboard)};
   
    connect(button_keyboard, &QPushButton::clicked, [=]() {
      RestoreActiveButtonLabel();
      button_keyboard->setText("[press key]");
      active_mapping = mapping;
      active_button = button_keyboard;
      waiting_for_keyboard = true;
    });
    
    layout->addWidget(button_keyboard, row, 1);
  }

  {
    button_controller = new QPushButton{GetControllerButtonName(mapping)};
    
    connect(button_controller, &QPushButton::clicked, [=]() {
      RestoreActiveButtonLabel();
      button_controller->setText("[press button]");
      active_mapping = mapping;
      active_button = button_controller;
      waiting_for_controller = true;
    });

    layout->addWidget(button_controller, row, 2);
  }

  {
    auto button = new QPushButton{tr("Clear")};

    connect(button, &QPushButton::clicked, [=]() {
      if(active_mapping == mapping) {
        waiting_for_keyboard = false;
        waiting_for_controller = false;
      }

      *mapping = {};
      config->Save();
      button_keyboard->setText(GetKeyboardButtonName(mapping->keyboard));
      button_controller->setText(GetControllerButtonName(mapping));
    });

    layout->addWidget(button, row, 3);
  }
}

void InputWindow::RestoreActiveButtonLabel() {
  if(waiting_for_keyboard) {
    active_button->setText(GetKeyboardButtonName(active_mapping->keyboard));
    waiting_for_keyboard = false;
  }
  
  if(waiting_for_controller) {
    active_button->setText(GetControllerButtonName(active_mapping));
    waiting_for_controller = false;
  }
}

auto InputWindow::GetKeyboardButtonName(int key) -> QString {
  if(key == 0) {
    return "None";
  }
  return QKeySequence{key}.toString();
}

auto InputWindow::GetControllerButtonName(QtConfig::Input::Map* mapping) -> QString {
  const auto button = mapping->controller.button;
  const auto axis = mapping->controller.axis;

  // const QString button_name = QString::asprintf("Button %d", button);
  // const QString axis_name = QString::asprintf("Axis%c %d", (axis & 0x80) ? '-' : '+', axis & ~0x80);

  const QString button_name = QStringLiteral("Button %1").arg(button);
  const QString axis_name = QStringLiteral("Axis%1 %2").arg(axis & 0x80 ? '-' : '+').arg(axis & ~0x80);

  // @todo: fix the conditions up
  if(button != SDL_CONTROLLER_BUTTON_INVALID && axis != SDL_CONTROLLER_AXIS_INVALID) {
    return QStringLiteral("%1 - %2").arg(button_name).arg(axis_name);
  } else if(button != SDL_CONTROLLER_BUTTON_INVALID) {
    return button_name;
  } else if(axis != SDL_CONTROLLER_AXIS_INVALID) {
    return axis_name;
  }

  return "None";
}