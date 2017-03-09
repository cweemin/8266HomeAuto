#ifndef __SWITCH__
#define __SWITCH__

#include <RCSwitch.h>

class switch_prop {
 public:
	 switch_prop(RCSwitch *rcswitch, int protocol, int nPulseLength, unsigned int length);
         void set();
 private: 
	 int _protocol;
	 int _nPulseLength;
	 unsigned int _length; 
         RCSwitch *_rcswitch;
 friend class pSwitch;
};

class pSwitch {
  public: 
     pSwitch(switch_prop *prop, unsigned long on_code, unsigned long off_code);
     void Toggle(bool);
     void On(void);
     void Off(void);
  private:
     switch_prop *_switch_prop;
     unsigned long _on_code;
     unsigned long _off_code;
};




#endif
