#include <algorithm>
#include <limits>
#include <cassert>

#include "device_port.h"

/* Basic Port member functions */
/* Constructor */
/* Default constructor */
BasicPort::BasicPort() {
  /* By default we set an invalid port, which size is 0 */
  lsb_ = 1;
  msb_ = 0;
}

/* Copy constructor */
BasicPort::BasicPort(const BasicPort& basic_port) {
  set(basic_port);
} 

/* Accessors */
/* get the port width */
size_t BasicPort::get_width() const {
  if (true == is_valid()) {
    return msb_ - lsb_ + 1; 
  }
  return 0; /* invalid port has a zero width */
}
 
/* get the LSB */
size_t BasicPort::get_msb() const {
  return msb_;
}

/* get the LSB */
size_t BasicPort::get_lsb() const {
  return lsb_; 
}

/* Mutators */
/* copy */
void BasicPort::set(const BasicPort& basic_port) {
  lsb_ = basic_port.get_lsb(); 
  msb_ = basic_port.get_msb(); 

  return;
}
 
/* set the port LSB and MSB */
void BasicPort::set_width(size_t width) {
  if (0 == width) {
    make_invalid();
    return;
  } 
  lsb_ = 0;
  msb_ = width - 1;
  return;
}
 
/* set the port LSB and MSB */
void BasicPort::set_width(size_t lsb, size_t msb) {
  /* If lsb and msb is invalid, we make a default port */
  if (lsb > msb) {
    make_invalid();
    return;
  } 
  set_lsb(lsb);
  set_msb(msb);
  return;
}
 
void BasicPort::set_lsb(size_t lsb) {
  lsb_ = lsb;
  return;
}

void BasicPort::set_msb(size_t msb) {
  msb_ = msb;
  return;
}

/* Increase the port width */
void BasicPort::expand(size_t width) { 
  if (0 == width) {
    return; /* ignore zero-width port */
  }
  /* If current port is invalid, we do not combine */
  if (0 == get_width()) {
    lsb_ = 0;
    msb_ = width;
    return;
  }
  /* Increase MSB */
  msb_ += width;
  return;
}

/* Swap lsb and msb */
void BasicPort::revert() {
  std::swap(lsb_, msb_);
  return;
}
 
/* rotate: increase both lsb and msb by an offset  */
bool BasicPort::rotate(size_t offset) {
  /* If current port is invalid or offset is 0, 
   * we do nothing 
   */
  if ((0 == offset) || (0 == get_width())) {
    return true;
  }
  /* check if leads to overflow: 
   * if limits - msb is larger than offset
   */
  if ( (std::numeric_limits<size_t>::max() - msb_ < offset) ) {
    return false;
  }
  /* Increase LSB and MSB */
  lsb_ += offset;
  msb_ += offset;
  return true;
}

/* rotate: decrease both lsb and msb by an offset  */
bool BasicPort::counter_rotate(size_t offset) {
  /* If current port is invalid or offset is 0, 
   * we do nothing 
   */
  if ((0 == offset) || (0 == get_width())) {
    return true;
  }
  /* check if leads to overflow: 
   * if limits is larger than offset
   */
  if ( (std::numeric_limits<size_t>::min() + lsb_  < offset) ) {
    return false;
  }
  /* decrease LSB and MSB */
  lsb_ -= offset;
  msb_ -= offset;
  return true;
}
 
/* Reset to initial port */
void BasicPort::reset() {
  make_invalid();
  return;
} 

/* Combine two ports */
void BasicPort::combine(const BasicPort& port) {
  /* LSB follows the current LSB */
  /* MSB increases */
  assert( 0 <  port.get_width() ); /* Make sure port is valid */
  /* If current port is invalid, we do not combine */
  if (0 == get_width()) {
    return;
  }
  /* Increase MSB */
  msb_ += port.get_width();
  return;
} 


/* Internal functions */
/* Make a port to be invalid: msb < lsb */
void BasicPort::make_invalid() {
  /* set a default invalid port */
  lsb_ = 1;
  msb_ = 0;
  return;
}

/* check if port size is valid > 0 */
bool BasicPort::is_valid() const {
  /* msb should be equal or greater than lsb, if this is a valid port */
  if ( msb_ < lsb_ ) {
    return false;
  }
  return true;
} 

/* ConfPorts member functions */
/* Constructor */
/* Default constructor */
ConfPorts::ConfPorts() { 
  /* default port */
  reserved_.reset();
  regular_.reset();
}

/* copy */
ConfPorts::ConfPorts(const ConfPorts& conf_ports) { 
  set(conf_ports);
}

/* Accessors */
size_t ConfPorts::get_reserved_port_width() const {
  return reserved_.get_width();
}

size_t ConfPorts::get_reserved_port_lsb() const {
  return reserved_.get_lsb();
}

size_t ConfPorts::get_reserved_port_msb() const {
  return reserved_.get_msb();
}

size_t ConfPorts::get_regular_port_width() const {
  return regular_.get_width();
}

size_t ConfPorts::get_regular_port_lsb() const {
  return regular_.get_lsb();
}

size_t ConfPorts::get_regular_port_msb() const {
  return regular_.get_msb();
}

/* Mutators */
void ConfPorts::set(const ConfPorts& conf_ports) {
  set_reserved_port(conf_ports.get_reserved_port_width());
  set_regular_port(conf_ports.get_regular_port_lsb(), conf_ports.get_regular_port_msb());
  return;
}

void ConfPorts::set_reserved_port(size_t width) {
  reserved_.set_width(width);
  return;
}

void ConfPorts::set_regular_port(size_t width) {
  regular_.set_width(width);
  return;
}

void ConfPorts::set_regular_port(size_t lsb, size_t msb) {
  regular_.set_width(lsb, msb);
  return;
}

void ConfPorts::set_regular_port_lsb(size_t lsb) {
  regular_.set_lsb(lsb);
  return;
}

void ConfPorts::set_regular_port_msb(size_t msb) {
  regular_.set_msb(msb);
  return;
}

/* Increase the port width of reserved port */
void ConfPorts::expand_reserved_port(size_t width) {
  reserved_.expand(width);
  return;
}
 
/* Increase the port width of regular port */
void ConfPorts::expand_regular_port(size_t width) { 
  regular_.expand(width);
  return;
}

/* Increase the port width of both ports */
void ConfPorts::expand(size_t width) { 
  expand_reserved_port(width);
  expand_regular_port(width);
}

/* rotate */
bool ConfPorts::rotate_regular_port(size_t offset) {
  return regular_.rotate(offset);
} 

/* counter rotate */
bool ConfPorts::counter_rotate_regular_port(size_t offset) {
  return regular_.counter_rotate(offset);
}

/* Reset to initial port */
void ConfPorts::reset() {
  reserved_.reset();
  regular_.reset();
  return;
} 



