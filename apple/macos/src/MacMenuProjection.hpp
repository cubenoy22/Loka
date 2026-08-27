#ifndef LOKA_MAC_MENU_PROJECTION_HPP
#define LOKA_MAC_MENU_PROJECTION_HPP

/** Owns the native main-menu graph and its action target as one projection. */
class MacMenuProjection
{
public:
  /** Adopts an Objective-C target already owned at +1 retain count. */
  explicit MacMenuProjection(void *ownedTarget);
  ~MacMenuProjection();

  void *target() const;
  /** Retains and installs the supplied NSMenu. */
  void install(void *menu);
  /** Detaches this projection's menu before releasing its retained graph. */
  void reset();

private:
  MacMenuProjection(const MacMenuProjection &);
  MacMenuProjection &operator=(const MacMenuProjection &);

  void *target_;
  void *menu_;
};

#endif // LOKA_MAC_MENU_PROJECTION_HPP
