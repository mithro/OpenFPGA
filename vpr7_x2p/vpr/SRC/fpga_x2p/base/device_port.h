/* IMPORTANT:
 * The following preprocessing flags are added to 
 * avoid compilation error when this headers are included in more than 1 times 
 */
#ifndef DEVICE_PORT_H
#define DEVICE_PORT_H

/* A basic port */
class BasicPort {
  public: /* Constructors */
    BasicPort();
    BasicPort(const BasicPort& basic_port); /* Copy constructor */
  public: /* Accessors */
    size_t get_width() const; /* get the port width */
    size_t get_msb() const; /* get the LSB */
    size_t get_lsb() const; /* get the LSB */
  public: /* Mutators */
    void set(const BasicPort& basic_port); /* copy */
    void set_width(size_t width); /* set the port LSB and MSB */
    void set_width(size_t lsb, size_t msb); /* set the port LSB and MSB */
    void set_lsb(size_t lsb);
    void set_msb(size_t msb);
    void expand(size_t width); /* Increase the port width */
    void revert(); /* Swap lsb and msb */
    bool rotate(size_t offset); /* rotate */
    bool counter_rotate(size_t offset); /* counter rotate */
    void reset(); /* Reset to initial port */
    void combine(const BasicPort& port); /* Combine two ports */
  private: /* internal functions */
    void make_invalid(); /* Make a port invalid */
    bool is_valid() const; /* check if port size is valid > 0 */
  private: /* Internal Data */
    size_t msb_; /* Most Significant Bit of this port */
    size_t lsb_; /* Least Significant Bit of this port */
};

/* Configuration ports:
 * 1. reserved configuration port, which is used by RRAM FPGA architecture
 * 2. regular configuration port, which is used by any FPGA architecture 
 */
class ConfPorts {
  public: /* Constructors */
    ConfPorts(); /* default port */
    ConfPorts(const ConfPorts& conf_ports); /* copy */
  public: /* Accessors */
    size_t get_reserved_port_width() const;
    size_t get_reserved_port_lsb() const;
    size_t get_reserved_port_msb() const;
    size_t get_regular_port_width() const;
    size_t get_regular_port_lsb() const;
    size_t get_regular_port_msb() const;
  public: /* Mutators */
    void set(const ConfPorts& conf_ports);
    void set_reserved_port(size_t width);
    void set_regular_port(size_t width);
    void set_regular_port(size_t lsb, size_t msb);
    void set_regular_port_lsb(size_t lsb);
    void set_regular_port_msb(size_t msb);
    void expand_reserved_port(size_t width); /* Increase the port width of reserved port */
    void expand_regular_port(size_t width); /* Increase the port width of regular port */
    void expand(size_t width); /* Increase the port width of both ports */
    bool rotate_regular_port(size_t offset); /* rotate */
    bool counter_rotate_regular_port(size_t offset); /* counter rotate */
    void reset(); /* Reset to initial port */
  private: /* Internal Data */
    BasicPort reserved_;
    BasicPort regular_;
};

/* TODO: create a class for BL and WL ports */

#endif

