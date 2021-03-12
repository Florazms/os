/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s2002579
 */
#include <infos/drivers/timer/rtc.h>
#include <infos/mm/mm.h>
#include <infos/kernel/sched.h>
#include <infos/util/lock.h>
#include <arch/x86/pio.h>
using namespace infos::kernel;
using namespace infos::util;
using namespace infos::arch::x86;
using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::mm;

class CMOSRTC : public RTC {
public:
	static const DeviceClass CMOSRTCDeviceClass;

	const DeviceClass& device_class() const override
	{
		return CMOSRTCDeviceClass;
	}

	int change_mode(int change){
		change = (change & 0x0F) + ((change/16)*10);
		return change;
	}

	// Check the mode of real time clock
	// Return true when RTC is binary, false when RCT is in BCD mode
	bool check_mode(){
		// Activate offset 0x0B
		__outb(0x70,0x0B);
		// Read data
		int value = __inb(0x71);
		if(value & 0x04){
			return true;
		}else{
			return false;
		}
	}	

	int short read_registers(int offset){
		__outb(0x70,offset);
		int value = __inb(0x71);
		return value;
	}               

	bool a_read(){
		// Activate offset 0x0A
		__outb(0x70,0x0A);
		int value = __inb(0x71);
		int flag = value & 0x80;
		return flag;
	}

	// Check the mode RTC time is 12 or 24 hours
	bool check_format(){
		// Activate offset 0x0B
		__outb(0x70,0x0B);
		int mode = __inb(0x71);
		// If 24 hour mode is set, return true or return false.
		if(mode & 0x02){
			return true;
		}else{
			return false;
		}
	}	

	/**
	 * Interrogates the RTC to read the current date & time.
	 * @param tp Populates the tp structure with the current data & time, as
	 * given by the CMOS RTC device.
	 */
	void read_timepoint(RTCTimePoint& tp) override
	{
		UniqueIRQLock l;
		while(a_read()){
			// do nothing, until the update process finished
		}

		bool twentyfour_hours = check_format();
		bool binary_mode = check_mode();
		
		mm_log.messagef(LogLevel::DEBUG, "twentyfour_hours is: %d:",twentyfour_hours);
		mm_log.messagef(LogLevel::DEBUG, "binary_mode is :%d",binary_mode);

		unsigned short temp_seconds = read_registers(0x00);
		unsigned short temp_minutes = read_registers(0x02);
		unsigned short temp_hours = read_registers(0x04);
		unsigned short temp_day_of_month = read_registers(0x07);
		unsigned short temp_month = read_registers(0x08);
		unsigned short temp_year = read_registers(0x09);

		int check_pm = (twentyfour_hours == false) && (temp_hours & 0x80);
		temp_hours = temp_hours & 0x7F;

		// Change mode for time, if they are not in binary mode.
		if(binary_mode == false){
			temp_seconds = change_mode(temp_seconds);
			temp_minutes = change_mode(temp_minutes);
			temp_hours = change_mode(temp_hours);
			temp_day_of_month = change_mode(temp_day_of_month);
			temp_month = change_mode(temp_month);
			temp_year = change_mode(temp_year);
		}
		
		if(check_pm)
			temp_hours = (temp_hours+12)%24;

		tp.seconds = temp_seconds;
		tp.minutes = temp_minutes;
		tp.hours = temp_hours;
		tp.day_of_month = temp_day_of_month;
		tp.month = temp_month;
		tp.year = temp_year;

		mm_log.messagef(LogLevel::DEBUG, "temp_seconds is: %d:",tp.seconds);
		mm_log.messagef(LogLevel::DEBUG, "temp_minutes is :%d",tp.minutes);
		mm_log.messagef(LogLevel::DEBUG, "temp_hours is :%d",tp.hours);
		mm_log.messagef(LogLevel::DEBUG, "temp_day_of_month is: %d",tp.day_of_month);
		mm_log.messagef(LogLevel::DEBUG, "temp_month is :%d",tp.month);
		mm_log.messagef(LogLevel::DEBUG, "temp_year is :%d",tp.year);
	}

};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
