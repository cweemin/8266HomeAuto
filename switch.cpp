#include "switch.h"

switch_prop::switch_prop(RCSwitch *rcswitch, int protocol, int nPulseLength, unsigned int length) {
  _rcswitch = rcswitch;
  _protocol = protocol;
  _nPulseLength = nPulseLength;
  _length = length;

}

void switch_prop::set() {
  _rcswitch->setProtocol(_protocol);
  _rcswitch->setPulseLength(_nPulseLength);
}

pSwitch::pSwitch(switch_prop *tmp_switch_prop, unsigned long on_code, unsigned long off_code, String tmp) {
   _on_code = on_code;
   _off_code  = off_code;
   _switch_prop = tmp_switch_prop;
   label = tmp;
}

void pSwitch::Toggle(bool onOff) {
   if (onOff)
      On();
   else 
      Off();
}
void pSwitch::On(void) {
   _switch_prop->set();
   _switch_prop->_rcswitch->send(_on_code, _switch_prop->_length);
//   ws->textAll(label+ " On");
}

void pSwitch::Off(void) {
   _switch_prop->set();
   //TODO: Need to pause receiver
   _switch_prop->_rcswitch->send(_off_code, _switch_prop->_length);
//   ws->textAll(label+ " Off");
}
