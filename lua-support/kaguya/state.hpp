// Copyright satoren
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <string>

#include "kaguya/config.hpp"

#include "kaguya/utility.hpp"
#include "kaguya/metatable.hpp"
#include "kaguya/error_handler.hpp"

#include "kaguya/lua_ref_table.hpp"
#include "kaguya/lua_ref_function.hpp"

namespace kaguya
{
	typedef std::pair<std::string, lua_CFunction> LoadLib;
	typedef std::vector<LoadLib> LoadLibs;
	inline LoadLibs NoLoadLib() { return LoadLibs(); }

	template<typename Allocator>
	void * AllocatorFunction(void *ud,
		void *ptr,
		size_t osize,
		size_t nsize)
	{
		Allocator* allocator = static_cast<Allocator*>(ud);
		if (!allocator)
		{
			return std::realloc(ptr, nsize);
		}
		if (nsize == 0)
		{
			allocator->deallocate(ptr, osize);
		}
		else if (ptr)
		{
			return allocator->reallocate(ptr, nsize);
		}
		else
		{
			return allocator->allocate(nsize);
		}
		return 0;
	}

	struct DefaultAllocator
	{
		typedef void* pointer;
		typedef size_t size_type;
		pointer allocate(size_type n)
		{
			return std::malloc(n);
		}
		pointer reallocate(pointer p, size_type n)
		{
			return std::realloc(p,n);
		}
		void deallocate(pointer p, size_type n)
		{
			std::free(p);
		}
	};

	class State
	{
		standard::shared_ptr<void> allocator_holder_;
		lua_State *state_;
		bool created_;

		//non copyable
		State(const State&);
		State& operator =(const State&);


		static int default_panic(lua_State *L) {
			fprintf(stderr, "PANIC: unprotected error in call to Lua API (%s)\n", lua_tostring(L, -1));
			fflush(stderr);
			return 0;  /* return to Lua to abort */
		}
		static void stderror_out(int status, const char* message)
		{
			std::cerr << message << std::endl;
		}
		void init()
		{
			if (!ErrorHandler::instance().getHandler(state_))
			{
				setErrorHandler(&stderror_out);
			}
			nativefunction::reg_functor_destructor(state_);
		}

	public:

		//! 

		/**
		* @name constructor
		* @brief create Lua state with lua standard library
		*/
		State() :allocator_holder_(), state_(luaL_newstate()), created_(true)
		{
			lua_atpanic(state_, &default_panic);
			init();
			openlibs();
		}

		/**
		* @name constructor
		* @brief create Lua state with lua standard library. Can not use this constructor at luajit. error message is 'Must use luaL_newstate() for 64 bit target'
		* @param allocator allocator for memory allocation @see DefaultAllocator
		*/
		template<typename Allocator>
		State(standard::shared_ptr<Allocator> allocator = standard::make_shared<Allocator>()) :allocator_holder_(allocator), state_(lua_newstate(&AllocatorFunction<Allocator>, allocator_holder_.get())), created_(true)
		{
			lua_atpanic(state_, &default_panic);
			init();
			openlibs();
		}

		/**
		* @name constructor
		* @brief create Lua state with (or without) libraries.
		* @param libs load libraries 
		* e.g. LoadLibs libs;libs.push_back(LoadLib("libname",libfunction));State state(libs);
		* e.g. State state({{"libname",libfunction}}); for c++ 11
		*/
		State(const LoadLibs& libs) : allocator_holder_(), state_(luaL_newstate()), created_(true)
		{
			lua_atpanic(state_, &default_panic);
			init();
			openlibs(libs);
		}
		/**
		* @name constructor
		* @brief create Lua state with (or without) libraries. Can not use this constructor at luajit. error message is 'Must use luaL_newstate() for 64 bit target'
		* @param allocator allocator for memory allocation @see DefaultAllocator
		*/
		template<typename Allocator>
		State(const LoadLibs& libs, standard::shared_ptr<Allocator> allocator = standard::make_shared<Allocator>()) : allocator_holder_(allocator), state_(lua_newstate(&AllocatorFunction<Allocator>, allocator_holder_.get())), created_(true)
		{
			lua_atpanic(state_, &default_panic);
			init();
			openlibs(libs);
		}


		/**
		* @name constructor
		* @brief construct using created lua_State. 
		* @param lua created lua_State. It is not call lua_close() in this class
		*/
		State(lua_State* lua) :state_(lua), created_(false)
		{
			init();
		}
		~State()
		{
			if (created_)
			{
				lua_close(state_);
			}
		}

		/**
		* @name setErrorHandler
		* @brief set error handler for lua error.
		*/
		void setErrorHandler(standard::function<void(int statuscode, const char*message)> errorfunction)
		{
			util::ScopedSavedStack save(state_);
			ErrorHandler::instance().registerHandler(state_, errorfunction);
		}


		/**
		* @name openlibs
		* @brief load all lua standard library
		*/
		void openlibs()
		{
			util::ScopedSavedStack save(state_);
			luaL_openlibs(state_);
		}

		/**
		* @name openlib
		* @brief load lua library
		*/
		void openlib(const LoadLib& lib)
		{
			util::ScopedSavedStack save(state_);

			luaL_requiref(state_, lib.first.c_str(), lib.second, 1);
		}
		
		/**
		* @name openlibs
		* @brief load lua libraries
		*/
		void openlibs(const LoadLibs& libs)
		{
			for (LoadLibs::const_iterator it = libs.begin(); it != libs.end(); ++it)
			{
				openlib(*it);
			}
		}

		/**
		* @name loadfile
		* @brief If there are no errors,compiled file as a Lua function and return.
		*  Otherwise send error message to error handler and return nil reference
		* @param file  file path of lua script
		* @return reference of lua function
		*/
		//@{
		LuaFunction loadfile(const std::string& file)
		{
			return LuaFunction::loadfile(state_, file);
		}
		LuaFunction loadfile(const char* file)
		{
			return LuaFunction::loadfile(state_, file);
		}
		//@}


		/**
		* @name loadstring
		* @brief If there are no errors,compiled string as a Lua function and return.
		*  Otherwise send error message to error handler and return nil reference
		* @param str lua code
		* @return reference of lua function
		*/
		//@{
		LuaFunction loadstring(const std::string& str)
		{
			return LuaFunction::loadstring(state_, str);
		}
		LuaFunction loadstring(const char* str)
		{
			return LuaFunction::loadstring(state_, str);
		}
		//@}

		/**
		* @name dofile
		* @brief Loads and runs the given file.
		* @param file file path of lua script
		* @param env execute env table
		* @return If there are no errors, returns true.Otherwise return false
		*/
		//@{
		bool dofile(const std::string& file, const LuaTable& env = LuaTable())
		{
			return dofile(file.c_str(), env);
		}
		bool dofile(const char* file, const LuaTable& env = LuaTable())
		{
			util::ScopedSavedStack save(state_);

			int status = luaL_loadfile(state_, file);

			if (status)
			{
				ErrorHandler::instance().handle(status, state_);
				return false;
			}

			if (!env.isNilref())
			{//register _ENV
				env.push();
#if LUA_VERSION_NUM >= 502
				lua_setupvalue(state_, -2, 1);
#else
				lua_setfenv(state_, -2);
#endif
			}

			status = lua_pcall_wrap(state_, 0, LUA_MULTRET);
			if (status)
			{
				ErrorHandler::instance().handle(status, state_);
				return false;
			}
			return true;
		}
		//@}

		/**
		* @name dostring
		* @brief Loads and runs the given string.
		* @param str lua script cpde
		* @param env execute env table
		* @return If there are no errors, returns true.Otherwise return false
		*/
		bool dostring(const char* str, const LuaTable& env = LuaTable())
		{
			util::ScopedSavedStack save(state_);

			int status = luaL_loadstring(state_, str);
			if (status)
			{
				ErrorHandler::instance().handle(status, state_);
				return false;
			}
			if (!env.isNilref())
			{//register _ENV
				env.push();
#if LUA_VERSION_NUM >= 502
				lua_setupvalue(state_, -2, 1);
#else
				lua_setfenv(state_, -2);
#endif
			}
			status = lua_pcall_wrap(state_, 0, LUA_MULTRET);
			if (status)
			{
				ErrorHandler::instance().handle(status, state_);
				return false;
			}
			return true;
		}
		bool dostring(const std::string& str, const LuaTable& env = LuaTable())
		{
			return dostring(str.c_str(), env);
		}
		bool operator()(const std::string& str)
		{
			return dostring(str);
		}
		bool operator()(const char* str)
		{
			return dostring(str);
		}
		//@}

		//! return element reference from global table
		TableKeyReference<std::string> operator[](const std::string& str)
		{
			int stack_top = lua_gettop(state_);
			lua_type_traits<GlobalTable>::push(state_, GlobalTable());
			int table_index = stack_top + 1;
			return TableKeyReference<std::string>(state_, table_index, str, stack_top, NoTypeCheck());
		}

		//! return element reference from global table
		TableKeyReference<const char*> operator[](const char* str)
		{
			int stack_top = lua_gettop(state_);
			lua_type_traits<GlobalTable>::push(state_, GlobalTable());
			int table_index = stack_top + 1;
			return TableKeyReference<const char*>(state_, table_index, str, stack_top, NoTypeCheck());
		}

		//! return global table
		LuaTable globalTable()
		{
			return newRef(GlobalTable());
		}

		//! return new Lua reference from argument value
		template<typename T>
		LuaRef newRef(const T& value)
		{
			return LuaRef(state_, value);
		}
#if KAGUYA_USE_CPP11
		//! return new Lua reference from argument value
		template<typename T>
		LuaRef newRef(T&& value)
		{
			return LuaRef(state_, std::forward<T>(value));
		}
#endif

		//! return new Lua table
		LuaTable newTable()
		{
			return LuaTable(state_);
		}
		//! return new Lua table
		LuaTable newTable(int reserve_array, int reserve_record)
		{
			return LuaTable(state_, NewTable(reserve_array, reserve_record));
		}

		//! create new Lua thread
		LuaThread newThread()
		{
			return LuaThread(state_);
		}
		//! create new Lua thread with function
		LuaThread newThread(const LuaFunction& f)
		{
			LuaThread cor(state_);
			cor.setFunction(f);
			return cor;
		}

		//push to Lua stack
		template<typename T>
		void pushToStack(T value)
		{
			lua_type_traits<T>::push(state_, value);
		}
		LuaRef popFromStack()
		{
			return LuaRef(state_, StackTop());
		}

		//! Garbage Collection of Lua 
		struct GCType
		{
			GCType(lua_State* state) :state_(state) {}

			/**
			* Performs a full garbage-collection cycle.
			*/
			void collect()
			{
				lua_gc(state_, LUA_GCCOLLECT, 0);
			}
			/** Performs an incremental step of garbage collection.
			* @return If returns true,the step finished a collection cycle.
			*/
			bool step()
			{
				return lua_gc(state_, LUA_GCSTEP, 0) == 1;
			}
			/**
			* Performs an incremental step of garbage collection.
			* @args the collector will perform as if that amount of memory (in KBytes) had been allocated by Lua.
			*/
			bool step(int size)
			{
				return lua_gc(state_, LUA_GCSTEP, size) == 1;
			}
			/**
			* enable gc
			*/
			void restart() { enable(); }
			/**
			* disable gc
			*/
			void stop() { disable(); }

			/**
			* returns the total memory in use by Lua in Kbytes.
			*/
			int count()const { return lua_gc(state_, LUA_GCCOUNT, 0); }

			/**
			* sets arg as the new value for the pause of the collector. Returns the previous value for pause.
			*/
			int steppause(int value) { return lua_gc(state_, LUA_GCSETPAUSE, value); }
			/**
			*  sets arg as the new value for the step multiplier of the collector. Returns the previous value for step.
			*/
			int setstepmul(int value) { return lua_gc(state_, LUA_GCSETSTEPMUL, value); }

			/**
			* enable gc
			*/
			void enable()
			{
				lua_gc(state_, LUA_GCRESTART, 0);
			}

			/**
			* disable gc
			*/
			void disable()
			{
				lua_gc(state_, LUA_GCSTOP, 0);
			}
#if LUA_VERSION_NUM >= 502
			/**
			* returns a boolean that tells whether the collector is running
			*/
			bool isrunning()const { return isenabled(); }
			/**
			* returns a boolean that tells whether the collector is running
			*/
			bool isenabled()const
			{
				return lua_gc(state_, LUA_GCISRUNNING, 0) != 0;
			}
#endif

		private:
			lua_State* state_;
		};

		//! return Garbage collection interface.
		GCType gc()const
		{
			return GCType(state_);
		}
		//! performs a full garbage-collection cycle.
		void garbageCollect()
		{
			gc().collect();
		}

		//! returns the current amount of memory (in Kbytes) in use by Lua.
		size_t useKBytes()const
		{
			return size_t(gc().count());
		}



		/**
		* @brief create Table and push to stack.
		* using for Lua module
		* @return return Lua Table Reference
		*/
		LuaRef newLib()
		{
			LuaTable newtable = newTable();
			newtable.push(state_);
			return newtable;
		}

		/**
		* @brief return lua_State*.
		* @return lua_State*
		*/
		lua_State *state() { return state_; };
	};
}
