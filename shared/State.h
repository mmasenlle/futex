
class State
{
public:
	static bool init(bool bwrite = false);
	static void close();
	
	static void set_input(int i, int v);
	static void set_output(int i, int v);
	static void set_ifstate(int i, int v); //boarding, checkin desk open, etc
	static void set_mstate(int i, int v); //state of the state machine (number)

	static int get_input(int i);
	static int get_output(int i);
	static int get_ifstate(int i);
	static int get_mstate(int i);
};

#ifndef NEVENTS

#define BOXSTATE_INIT(_b) State::init(_b)
#define BOXSTATE_CLOSE() State::close()
#define BOXSTATE_SET_INPUT(_i, _v) State::set_input(_i, _v)
#define BOXSTATE_SET_OUTPUT(_i, _v) State::set_output(_i, _v)
#define BOXSTATE_SET_IFSTATE(_i, _v) State::set_ifstate(_i, _v)
#define BOXSTATE_SET_MSTATE(_i, _v) State::set_mstate(_i, _v)
#define BOXSTATE_GET_INPUT(_i) State::get_input(_i)
#define BOXSTATE_GET_OUTPUT(_i) State::get_output(_i)
#define BOXSTATE_GET_IFSTATE(_i) State::get_ifstate(_i)
#define BOXSTATE_GET_MSTATE(_i) State::get_mstate(_i)

#else

#define BOXSTATE_INIT(_b) (false)
#define BOXSTATE_CLOSE()
#define BOXSTATE_SET_INPUT(_i, _v)
#define BOXSTATE_SET_OUTPUT(_i, _v)
#define BOXSTATE_SET_IFSTATE(_i, _v)
#define BOXSTATE_SET_MSTATE(_i, _v)
#define BOXSTATE_GET_INPUT(_i) (-1)
#define BOXSTATE_GET_OUTPUT(_i) (-1)
#define BOXSTATE_GET_IFSTATE(_i) (-1)
#define BOXSTATE_GET_MSTATE(_i) (-1)

#endif
