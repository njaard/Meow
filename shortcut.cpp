#include "shortcut.h"
#include <qevent.h>
#include <qabstracteventdispatcher.h>
#include <qmessagebox.h>

#include <algorithm>

#ifdef _WIN32
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <winuser.h>
#endif

#ifdef Q_WS_X11
#include <qx11info_x11.h>
#include <X11/Xlib.h>
#endif


#if defined(_WIN32)
int Meow::Shortcut::nextId=0;
#elif defined(Q_WS_X11)
static bool meowShortcutError=false;


static int xErrorHandler(Display*, XErrorEvent *event)
{
	if (event->error_code == BadAccess || event->error_code == BadValue || event->error_code == BadWindow)
	{
		if (event->request_code == 33 || event->request_code == 34)
		{
			meowShortcutError = true;
		}
    }
    return 0;
}

#endif

static const Qt::KeyboardModifiers allMods = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;

std::list<Meow::Shortcut*>* Meow::Shortcut::allShortcuts=0;


bool Meow::Shortcut::globalEventFilter(void *_m)
{
#if defined(_WIN32)
	MSG *const m = (MSG*)_m;
	if (m->message == WM_HOTKEY)
	{
		const unsigned keycode = HIWORD(m->lParam);
		const unsigned keymod = LOWORD(m->lParam) & (MOD_SHIFT | MOD_CONTROL | MOD_ALT | MOD_WIN);
		for (std::list<Shortcut*>::iterator i = allShortcuts->begin(); i != allShortcuts->end(); ++i)
		{
			if ((*i)->maybeActivate(keycode, keymod))
				break;
		}
	}
#elif defined(Q_WS_X11)
	XEvent* event = static_cast<XEvent*>(_m);
	if (event->type == KeyPress)
	{
		Display* const display = QX11Info::display();
		XKeyEvent* const key = (XKeyEvent*)event;
		const unsigned keycode = key->keycode;
		const unsigned keymod = key->state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
		for (std::list<Shortcut*>::iterator i = allShortcuts->begin(); i != allShortcuts->end(); ++i)
		{
			if ((*i)->maybeActivate(keycode, keymod))
				break;
		}
	}
#endif
	return false;
}

Meow::Shortcut::Shortcut(QWidget *parent)
	: QObject(parent)
{
	if (!allShortcuts)
	{
#if defined(_WIN32)
		parentWindowHandle = parent->winId();
#endif
		QAbstractEventDispatcher::instance()->setEventFilter(globalEventFilter);
		allShortcuts = new std::list<Shortcut*>;
	}
	allShortcuts->push_back(this);
}


Meow::Shortcut::Shortcut(const QKeySequence &k, QWidget *parent)
	: QObject(parent)
{
	if (!allShortcuts)
	{
#if defined(_WIN32)
		parentWindowHandle = parent->winId();
#endif
		QAbstractEventDispatcher::instance()->setEventFilter(globalEventFilter);
		allShortcuts = new std::list<Shortcut*>;
	}
	allShortcuts->push_back(this);
	setKey(k);
}

Meow::Shortcut::~Shortcut()
{
	allShortcuts->erase(std::find(allShortcuts->begin(), allShortcuts->end(), this));
	setKey(QKeySequence());
}

bool Meow::Shortcut::maybeActivate(unsigned keycode, unsigned keymod)
{
	if (mKey.isEmpty())
		return false;
	unsigned nc = nativeKeycode(Qt::Key(mKey[0] & ~allMods));
	unsigned nm = nativeModifier(Qt::KeyboardModifiers(mKey[0] & allMods));
	if (nc == keycode && nm == keymod)
	{
		emit activated();
		return true;
	}
	return false;
}

bool Meow::Shortcut::setKey(const QKeySequence &k)
{
	if (mKey == k) return true;
	if (!mKey.isEmpty())
	{
#if defined(_WIN32)
		UnregisterHotKey(parentWindowHandle, id);
#elif defined(Q_WS_X11)
		Display* const display = QX11Info::display();
		const Window window = QX11Info::appRootWindow();
		unsigned nc = nativeKeycode(Qt::Key(mKey[0] & ~allMods));
		unsigned nm = nativeModifier(Qt::KeyboardModifiers(mKey[0] & allMods));
		XUngrabKey(display, nc, nm, window);
#endif
	}
	mKey = k;
	if (!mKey.isEmpty())
	{
		// see if another Shortcut in this process already registered me
		for (std::list<Shortcut*>::iterator i = allShortcuts->begin(); i != allShortcuts->end(); ++i)
		{
			if (*i != this && (*i)->key() == mKey)
			{
				mKey = mKey = QKeySequence();
				return false;
			}
		}
		
		unsigned nc = nativeKeycode(Qt::Key(mKey[0] & ~allMods));
		unsigned nm = nativeModifier(Qt::KeyboardModifiers(mKey[0] & allMods));
#if defined(_WIN32)
		if (!RegisterHotKey(parentWindowHandle, id = nextId++, nm, nc))
		{
			mKey = QKeySequence();
			return false;
		}
#elif defined(Q_WS_X11)
		Display* const display = QX11Info::display();
		const Window window = QX11Info::appRootWindow();
		XSync(display, False);
		meowShortcutError = false;
		int (*const oldXErrorHandler)(Display* display, XErrorEvent* event) = XSetErrorHandler(xErrorHandler);
		XGrabKey(display, nc, nm, window, true, GrabModeAsync, GrabModeAsync);
		XGrabKey(display, nc, nm|Mod2Mask, window, true, GrabModeAsync, GrabModeAsync); // numlock?
		XSync(display, False);
		XSetErrorHandler(oldXErrorHandler);
		if (meowShortcutError)
			mKey = QKeySequence();
		return !meowShortcutError;
#endif
	}
	return true;
}



Meow::ShortcutInput::ShortcutInput(Shortcut *s,QWidget *parent)
	: QPushButton(parent), mShortcut(s), inputtingShortcut(false)
{
	mKey = s->key();
	if (s->key().isEmpty())
		setText("Set Shortcut");
	else
		setText(s->key().toString(QKeySequence::NativeText));
	
	connect(this, SIGNAL(clicked()), SLOT(beginGrab()));
}

Meow::ShortcutInput::~ShortcutInput()
{
	releaseKeyboard();
}

void Meow::ShortcutInput::beginGrab()
{
	setText("Input Shortcut...");
	grabKeyboard();
	inputtingShortcut = true;
}

bool Meow::ShortcutInput::event(QEvent *e)
{
	if (inputtingShortcut && e->type() == 6) // QEvent::KeyPress
	{
		QKeyEvent *const ke = static_cast<QKeyEvent*>(e);
		bool done=true;
		int k = ke->key();
		if (ke->key() == Qt::Key_Shift
			|| ke->key() == Qt::Key_Control
			|| ke->key() == Qt::Key_Alt
			|| ke->key() == Qt::Key_Meta
			|| ke->key() == Qt::Key_AltGr)
		{
			k=0;
			done=false;
		}
		QKeySequence seq(k|ke->modifiers());
		setText(seq.toString(QKeySequence::NativeText));
		if (done)
		{
			inputtingShortcut=false;
			releaseKeyboard();
			
			QKeySequence old = mShortcut->key();
			mShortcut->setKey(QKeySequence());
			bool ok;
			{
				Shortcut c(this);
				ok = c.setKey(seq);
			}
			
			mShortcut->setKey(old);
			if (!ok)
			{
				setText(old.toString(QKeySequence::NativeText));
				QMessageBox::information(0, tr("Shortcut error"), tr("Failed to create shortcut %1, perhaps it is already taken").arg(seq.toString()));
			}
			else
			{
				mKey = seq;
				emit changed();
				emit changed(seq);
			}
		}
		return true;
	}
	return QWidget::event(e);
}


unsigned int Meow::Shortcut::nativeKeycode(Qt::Key key)
{
#if defined(_WIN32)
	switch (key)
	{
	case Qt::Key_Escape:
		return VK_ESCAPE;
	case Qt::Key_Tab:
	case Qt::Key_Backtab:
		return VK_TAB;
	case Qt::Key_Backspace:
		return VK_BACK;
	case Qt::Key_Return:
	case Qt::Key_Enter:
		return VK_RETURN;
	case Qt::Key_Insert:
		return VK_INSERT;
	case Qt::Key_Delete:
		return VK_DELETE;
	case Qt::Key_Pause:
		return VK_PAUSE;
	case Qt::Key_Print:
		return VK_PRINT;
	case Qt::Key_Clear:
		return VK_CLEAR;
	case Qt::Key_Home:
		return VK_HOME;
	case Qt::Key_End:
		return VK_END;
	case Qt::Key_Left:
		return VK_LEFT;
	case Qt::Key_Up:
		return VK_UP;
	case Qt::Key_Right:
		return VK_RIGHT;
	case Qt::Key_Down:
		return VK_DOWN;
	case Qt::Key_PageUp:
		return VK_PRIOR;
	case Qt::Key_PageDown:
		return VK_NEXT;
	case Qt::Key_F1:
		return VK_F1;
	case Qt::Key_F2:
		return VK_F2;
	case Qt::Key_F3:
		return VK_F3;
	case Qt::Key_F4:
		return VK_F4;
	case Qt::Key_F5:
		return VK_F5;
	case Qt::Key_F6:
		return VK_F6;
	case Qt::Key_F7:
		return VK_F7;
	case Qt::Key_F8:
		return VK_F8;
	case Qt::Key_F9:
		return VK_F9;
	case Qt::Key_F10:
		return VK_F10;
	case Qt::Key_F11:
		return VK_F11;
	case Qt::Key_F12:
		return VK_F12;
	case Qt::Key_F13:
		return VK_F13;
	case Qt::Key_F14:
		return VK_F14;
	case Qt::Key_F15:
		return VK_F15;
	case Qt::Key_F16:
		return VK_F16;
	case Qt::Key_F17:
		return VK_F17;
	case Qt::Key_F18:
		return VK_F18;
	case Qt::Key_F19:
		return VK_F19;
	case Qt::Key_F20:
		return VK_F20;
	case Qt::Key_F21:
		return VK_F21;
	case Qt::Key_F22:
		return VK_F22;
	case Qt::Key_F23:
		return VK_F23;
	case Qt::Key_F24:
		return VK_F24;
	case Qt::Key_Space:
		return VK_SPACE;
	case Qt::Key_Asterisk:
		return VK_MULTIPLY;
	case Qt::Key_Plus:
		return VK_ADD;
	case Qt::Key_Comma:
		return VK_SEPARATOR;
	case Qt::Key_Minus:
		return VK_SUBTRACT;
	case Qt::Key_Slash:
		return VK_DIVIDE;
	case Qt::Key_MediaNext:
		return VK_MEDIA_NEXT_TRACK;
	case Qt::Key_MediaPrevious:
		return VK_MEDIA_PREV_TRACK;
	case Qt::Key_MediaPlay:
		return VK_MEDIA_PLAY_PAUSE;
	case Qt::Key_MediaStop:
		return VK_MEDIA_STOP;
	//case Qt::Key_MediaLast:
	//case Qt::Key_MediaRecord:
	case Qt::Key_VolumeDown:
		return VK_VOLUME_DOWN;
	case Qt::Key_VolumeUp:
		return VK_VOLUME_UP;
	case Qt::Key_VolumeMute:
		return VK_VOLUME_MUTE;
	case Qt::Key_LaunchMedia:
		return VK_LAUNCH_MEDIA_SELECT;
	
	default:
		if (key >= Qt::Key_0 && key <= Qt::Key_9)
			return key;
		if (key >= Qt::Key_A && key <= Qt::Key_Z)
			return key;
		return 0;
	}
#elif defined(Q_WS_X11)
	Display* display = QX11Info::display();
	switch (key)
	{
	case Qt::Key_MediaPlay:
		return XKeysymToKeycode(display, 0x1008ff14);
	case Qt::Key_MediaStop:
		return XKeysymToKeycode(display, 0x1008ff15);
	case Qt::Key_MediaPrevious:
		return XKeysymToKeycode(display, 0x1008ff16);
	case Qt::Key_MediaNext:
		return XKeysymToKeycode(display, 0x1008ff17);
	case Qt::Key_VolumeDown:
		return XKeysymToKeycode(display, 0x1008ff11);
	case Qt::Key_VolumeUp:
		return XKeysymToKeycode(display, 0x1008ff13);
	case Qt::Key_VolumeMute:
		return XKeysymToKeycode(display, 0x1008ff12);
	case Qt::Key_MediaPause:
		return XKeysymToKeycode(display, 0x1008FF31);
	case Qt::Key_LaunchMedia:
		return XKeysymToKeycode(display, 0x1008FF32);
	default:
		return XKeysymToKeycode(display, XStringToKeysym(QKeySequence(key).toString().toLatin1().constData()));
	}
#endif
}

unsigned int Meow::Shortcut::nativeModifier(Qt::KeyboardModifiers modifiers)
{
	unsigned native = 0;
#if defined(_WIN32)
	if (modifiers & Qt::ShiftModifier)
		native |= MOD_SHIFT;
	if (modifiers & Qt::ControlModifier)
		native |= MOD_CONTROL;
	if (modifiers & Qt::AltModifier)
		native |= MOD_ALT;
	if (modifiers & Qt::MetaModifier)
		native |= MOD_WIN;
#elif defined(Q_WS_X11)
	if (modifiers & Qt::ShiftModifier)
		native |= ShiftMask;
	if (modifiers & Qt::ControlModifier)
		native |= ControlMask;
	if (modifiers & Qt::AltModifier)
		native |= Mod1Mask;
	if (modifiers & Qt::MetaModifier)
		native |= Mod4Mask;
#endif
	return native;
}



// kate: space-indent off; replace-tabs off;
