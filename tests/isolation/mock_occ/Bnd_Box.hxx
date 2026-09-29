#pragma once
struct Bnd_Box {
  bool void_{true}, open_{false}; double v[6]{};
  static Bnd_Box Make(double a, double b, double c, double d, double e, double f) { Bnd_Box x; x.void_ = false; x.v[0]=a;x.v[1]=b;x.v[2]=c;x.v[3]=d;x.v[4]=e;x.v[5]=f; return x; }
  bool IsVoid() const { return void_; } bool IsOpen() const { return open_; }
  void Get(double &a, double &b, double &c, double &d, double &e, double &f) const { a=v[0];b=v[1];c=v[2];d=v[3];e=v[4];f=v[5]; }
};
