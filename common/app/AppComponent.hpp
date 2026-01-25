#ifndef LOKA_APPCOMPONENT_HPP
#define LOKA_APPCOMPONENT_HPP

class Window;

// --- AppComponent: アプリ全体の構成要素の基底クラス（今後拡張可） ---
class AppComponent
{
public:
  virtual ~AppComponent() {}

  // Type cast (avoid dynamic_cast for 68k performance)
  virtual Window *asWindow() { return 0; }
};

#endif // LOKA_APPCOMPONENT_HPP
