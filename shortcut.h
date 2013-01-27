#ifndef SHORTCUTINPUT_H
#define SHORTCUTINPUT_H

#include <qpushbutton.h>

#include <list>

namespace Meow
{


class Shortcut : public QObject
{
	Q_OBJECT
	QKeySequence mKey;
#if defined(_WIN32)
	HWND parentWindowHandle;
	int id;
	static int nextId;
#endif
	static std::list<Shortcut*> *allShortcuts;
	
public:
	Shortcut(QWidget *parent);
	Shortcut(const QKeySequence &key, QWidget *parent);

	~Shortcut();
	
	bool setKey(const QKeySequence &k);
	
	QKeySequence key() const { return mKey; }
	
signals:
	void activated();

private:
	static unsigned int nativeKeycode(Qt::Key key);
	static unsigned int nativeModifier(Qt::KeyboardModifiers modifiers);
	static bool globalEventFilter(void *_m);
	bool maybeActivate(unsigned keycode, unsigned keymod);
};

class ShortcutInput : public QPushButton
{
	Q_OBJECT
	Shortcut *mShortcut;
	QKeySequence mKey;
	bool inputtingShortcut;
	
public:
	ShortcutInput(Shortcut *shortcut, QWidget *parent);
	~ShortcutInput();
	
	QKeySequence key() const { return mKey; }
	
signals:
	void changed();
	void changed(const QKeySequence &k);
	void nativeChanged(unsigned vkey, unsigned mod);

protected:
	bool event(QEvent *e);
	
private slots:
	void beginGrab();
};


}

#endif

// kate: space-indent off; replace-tabs off;
