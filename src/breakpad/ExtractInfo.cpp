/*
 *  ExtractInfo.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 18.10.09.
 *  Code under LGPL.
 *
 */

#include <sstream>
#include <iomanip>
#include <fstream>

#include "ExtractInfo.h"
#include "Debug.h"
#include "ThreadPool.h" // InitThreadPool, threadPool, UnInitThreadPool
#include "Networking.h"
#include "Options.h"
#include "Version.h"
#include "SMTP.h"
#include "FindFile.h"
#include "CGameMode.h"
#include "TaskManager.h"

// defined in main.cpp
void setBinaryDirAndName(char* argv0);

static bool MiniInit() {
	hints << "Minimal initialisation of " << GetFullGameName() << " ..." << endl;
	
	InitThreadPool(2);
	
	if(!InitNetworkSystem()) {
		errors << "Failed to initialize the network library" << endl;
		return false;
	}

	// just to avoid some errors in GameOptions::Init, not really needed otherwise
	InitGameModes();

	InitTaskManager();	

	if(!GameOptions::Init()) {
		errors << "Could not load options" << endl;
		return false;
	}
		
	return true;
}

static void MiniUninit() {
	taskManager->finishQueuedTasks();
	threadPool->waitAll(); // do that before uniniting task manager because some threads could access it

	UnInitTaskManager();

	UnInitThreadPool();

	// Network
	QuitNetworkSystem();

	notes << "Good Bye and enjoy your day..." << endl;
}

#ifndef _MSC_VER
extern char **environ;
#endif

int DoCrashReport(int argc, char** argv) {
	if(argc < 2) return 0;
	if(std::string(argv[1]) != "-crashreport") return 0;
	
	notes << "exec:";
	for(int i = 0; i < argc; ++i)
		notes << " \"" << argv[i] << "\"";
	notes << endl;
	
	if(argc < 5) {
		errors << "usage: " << argv[0] << " -crashreport <dumpdir> <dumpid> <logfile> [-debug]" << endl;
		return 1;
	}

	setBinaryDirAndName(argv[0]);
	struct OlxSystem {
		bool init() { return MiniInit(); }
		~OlxSystem() { MiniUninit(); }
	} olx;
	if(!olx.init()) return 1;

	std::string minidumpfile = std::string(argv[2]) + "/" + argv[3] + ".dmp";
	std::string logfilename = argv[4];
	
	// collect crashinfo before connecting to SMTP to avoid timeout on SMTP server
	hints << "generating crash report ..." << endl;
	std::stringstream crashinfo, crashinfoerror;
	// NOTE: Breakpad expects a systemnative path. 
	// minidumpfile is from cmd parameter, so it is already systemnative.
	MinidumpExtractInfo(minidumpfile, crashinfo, crashinfoerror);

	if(argc >= 6 && strcmp(argv[5], "-debug") == 0) {
		std::cout << crashinfo.str() << std::endl;
		std::cout << "----" << std::endl;
		std::cout << crashinfoerror.str() << std::endl;
		std::cout << "---- EOF" << std::endl;
		return 1;
	}
	
	SmtpClient smtp;
	smtp.host = "mail.az2000.de:25";
	smtp.mailfrom = "olxcrash@az2000.de";
	smtp.mailrcpts.push_back( "olxcrash@az2000.de" );
	smtp.subject = GetGameVersion().asHumanString() + " crash report";

	if(!smtp.connect()) {
		errors << "could not connect to SMTP server" << endl;
		return 1;
	}
	hints << "Connected to SMTP server" << endl;
	
	smtp.addText("\nSystem environment:\n\n");
	for(char** env = environ; *env != NULL; ++env)
		smtp.addText(*env);
	hints << "System environment sent" << endl;

	
	smtp.addText("\n\n\n\nCrash information:\n\n");
	crashinfo << std::flush;
	smtp.addText(crashinfo.str());
	hints << "Crash information sent" << endl;

	
	smtp.addText("\n\n\n\n\nRecent console output:\n");
	smtp.addText("File: " + logfilename + "\n\n");
	// We got the filename from cmd param, so it is already in native, so no Utf8ToSystemNative needed
	std::ifstream logfile(logfilename.c_str(), std::ios_base::in);
	if(logfile) {
		std::string line;
		while(getline(logfile, line, '\n'))
			smtp.addText(line);
		logfile.close();
	}
	else {
		smtp.addText("Logfile could not be read!\n");
		warnings << "Logfile could not be read" << endl;
	}
	hints << "Logfile sent" << endl;
	

	smtp.addText("\n\n\n\nCurrent config:\n");
	std::string cfgfilename = GetFullFileName("cfg/options.cfg");
	smtp.addText("File: " + cfgfilename + "\n\n");
	std::ifstream cfgfile(Utf8ToSystemNative(cfgfilename).c_str(), std::ios_base::in);
	if(cfgfile) {
		std::string line;
		while(getline(cfgfile, line, '\n')) {
			if(stringcasefind(line, "password") != std::string::npos)
				smtp.addText("# <password was here>");
			else
				smtp.addText(line);
		}
		cfgfile.close();
	}
	else {
		smtp.addText("Config file could not be read!");
		warnings << "Configfile could not be read" << endl;
	}
	hints << "Configfile sent" << endl;
	
	
	if(!smtp.close())
		errors << "Error while closing SMTP" << endl;
	
	hints << "Everything ready, quitting" << endl;
	return 1;
}

#include <iostream>
#include "BreakPad.h"

#ifndef NBREAKPAD

#include <cstring>

#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/minidump.h"
#include "google_breakpad/processor/minidump_processor.h"
#include "google_breakpad/processor/process_state.h"
#include "google_breakpad/processor/stack_frame_cpu.h"
#include "google_breakpad/processor/system_info.h"
#include "processor/pathname_stripper.h"
#include "processor/scoped_ptr.h"
#include "processor/simple_symbol_supplier.h"

#include "on_demand_symbol_supplier.h"

using std::string;

using google_breakpad::BasicSourceLineResolver;
using google_breakpad::CallStack;
using google_breakpad::CodeModule;
using google_breakpad::CodeModules;
using google_breakpad::Minidump;
using google_breakpad::MinidumpProcessor;
using google_breakpad::OnDemandSymbolSupplier;
using google_breakpad::PathnameStripper;
using google_breakpad::ProcessState;
using google_breakpad::scoped_ptr;
using google_breakpad::StackFrame;
using google_breakpad::StackFramePPC;
using google_breakpad::StackFrameX86;
using google_breakpad::SystemInfo;


//=============================================================================
static int PrintRegister(const char *name, u_int32_t value, int sequence, std::ostream& out, std::ostream& err) {
	if (sequence % 4 == 0) {
		out << std::endl;
	}
	char tmp[30];
	sprintf(tmp, "%6s = 0x%08x ", name, value);
	out << tmp;
	return ++sequence;
}

//=============================================================================
static void PrintStack(const CallStack *stack, const string &cpu, std::ostream& out, std::ostream& err) {
	int frame_count = (int)stack->frames()->size();
	char buffer[1024];
	for (int frame_index = 0; frame_index < frame_count; ++frame_index) {
		const StackFrame *frame = stack->frames()->at(frame_index);
		const CodeModule *module = frame->module;
		char tmp[10];
		sprintf(tmp, "%2d", frame_index);
		out << tmp << " ";
		
		if (module) {
			// Module name (20 chars max)
			strcpy(buffer, PathnameStripper::File(module->code_file()).c_str());
			int maxStr = 20;
			buffer[maxStr] = 0;
			char tmp[2048];
			sprintf(tmp, "%-*s", maxStr, buffer);
			out << tmp;

			maxStr = 10;
			strcpy(buffer, module->version().c_str());
			buffer[maxStr] = 0;
			sprintf(tmp, "%-*s",maxStr, buffer);
			out << tmp;
			
			u_int64_t instruction = frame->instruction;
			
			// PPC only: Adjust the instruction to match that of Crash reporter.  The
			// instruction listed is actually the return address.  See the detailed
			// comments in stackwalker_ppc.cc for more information.
			if (cpu == "ppc" && frame_index)
				instruction += 4;
			
			sprintf(tmp, " 0x%08llx (0x%08llx) ", instruction, instruction - module->base_address());
			out << tmp;
			
			// Function name
			if (!frame->function_name.empty())
				out << frame->function_name;
			else
				out << "??";

			if (!frame->source_file_name.empty()) {
				string source_file = PathnameStripper::File(frame->source_file_name);
				sprintf(tmp, " + 0x%llx (%s:%d)",
						instruction - frame->source_line_base,
						source_file.c_str(), frame->source_line);
				out << tmp;
			} else if(frame->function_base > 0) {
				sprintf(tmp, " + 0x%llx", instruction - frame->function_base);
				out << tmp;
			}
			
		}
		out << std::endl;
	}
}

//=============================================================================
static void PrintRegisters(const CallStack *stack, const string &cpu, std::ostream& out, std::ostream& err) {
	int sequence = 0;
	const StackFrame *frame = stack->frames()->at(0);
	if (cpu == "x86") {
		const StackFrameX86 *frame_x86 =
		reinterpret_cast<const StackFrameX86*>(frame);
		
		if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EIP)
			sequence = PrintRegister("eip", frame_x86->context.eip, sequence, out, err);
		if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_ESP)
			sequence = PrintRegister("esp", frame_x86->context.esp, sequence, out, err);
		if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EBP)
			sequence = PrintRegister("ebp", frame_x86->context.ebp, sequence, out, err);
		if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EBX)
			sequence = PrintRegister("ebx", frame_x86->context.ebx, sequence, out, err);
		if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_ESI)
			sequence = PrintRegister("esi", frame_x86->context.esi, sequence, out, err);
		if (frame_x86->context_validity & StackFrameX86::CONTEXT_VALID_EDI)
			sequence = PrintRegister("edi", frame_x86->context.edi, sequence, out, err);
		if (frame_x86->context_validity == StackFrameX86::CONTEXT_VALID_ALL) {
			sequence = PrintRegister("eax", frame_x86->context.eax, sequence, out, err);
			sequence = PrintRegister("ecx", frame_x86->context.ecx, sequence, out, err);
			sequence = PrintRegister("edx", frame_x86->context.edx, sequence, out, err);
			sequence = PrintRegister("efl", frame_x86->context.eflags, sequence, out, err);
		}
	} else if (cpu == "ppc") {
		const StackFramePPC *frame_ppc =
		reinterpret_cast<const StackFramePPC*>(frame);
		
		if ((frame_ppc->context_validity & StackFramePPC::CONTEXT_VALID_ALL) ==
			StackFramePPC::CONTEXT_VALID_ALL) {
			sequence = PrintRegister("srr0", frame_ppc->context.srr0, sequence, out, err);
			sequence = PrintRegister("srr1", frame_ppc->context.srr1, sequence, out, err);
			sequence = PrintRegister("cr", frame_ppc->context.cr, sequence, out, err);
			sequence = PrintRegister("xer", frame_ppc->context.xer, sequence, out, err);
			sequence = PrintRegister("lr", frame_ppc->context.lr, sequence, out, err);
			sequence = PrintRegister("ctr", frame_ppc->context.ctr, sequence, out, err);
			sequence = PrintRegister("mq", frame_ppc->context.mq, sequence, out, err);
			sequence = PrintRegister("vrsave", frame_ppc->context.vrsave, sequence, out, err);
			
			sequence = 0;
			char buffer[5];
			for (int i = 0; i < MD_CONTEXT_PPC_GPR_COUNT; ++i) {
				sprintf(buffer, "r%d", i);
				sequence = PrintRegister(buffer, frame_ppc->context.gpr[i], sequence, out, err);
			}
		} else {
			if (frame_ppc->context_validity & StackFramePPC::CONTEXT_VALID_SRR0)
				sequence = PrintRegister("srr0", frame_ppc->context.srr0, sequence, out, err);
			if (frame_ppc->context_validity & StackFramePPC::CONTEXT_VALID_GPR1)
				sequence = PrintRegister("r1", frame_ppc->context.gpr[1], sequence, out, err);
		}
	}
	
	out << std::endl;
}

static void PrintModules(const CodeModules *modules, std::ostream& out, std::ostream& err) {
	if (!modules)
		return;
	
	out << std::endl;
	out << "Loaded modules:" << std::endl;
	
	u_int64_t main_address = 0;
	const CodeModule *main_module = modules->GetMainModule();
	if (main_module) {
		main_address = main_module->base_address();
	}
	
	unsigned int module_count = modules->module_count();
	for (unsigned int module_sequence = 0;
		 module_sequence < module_count;
		 ++module_sequence) {
		const CodeModule *module = modules->GetModuleAtSequence(module_sequence);
		assert(module);
		u_int64_t base_address = module->base_address();
		char tmp[500];
		sprintf(tmp, "0x%08llx - 0x%08llx  %s  %s%s  %s\n",
			   base_address, base_address + module->size() - 1,
			   PathnameStripper::File(module->code_file()).c_str(),
			   module->version().empty() ? "???" : module->version().c_str(),
			   main_module != NULL && base_address == main_address ?
			   "  (main)" : "",
			   module->code_file().c_str());
		out << tmp << std::flush;
	}
}

static void PrintThread(const CallStack *stack, const string &cpu, std::ostream& out, std::ostream& err) {
	PrintStack(stack, cpu, out, err);
	PrintRegisters(stack, cpu, out, err);
}

	

static void ProcessSingleReport(const std::string& minidump_file, std::ostream& out, std::ostream& err) {
	BasicSourceLineResolver resolver;
	string search_dir = "";
	string symbol_search_dir = "";
	scoped_ptr<OnDemandSymbolSupplier> symbol_supplier(
													   new OnDemandSymbolSupplier(search_dir, symbol_search_dir));
	scoped_ptr<MinidumpProcessor>
    minidump_processor(new MinidumpProcessor(symbol_supplier.get(), &resolver));
	ProcessState process_state;
	scoped_ptr<Minidump> dump(new google_breakpad::Minidump(minidump_file));
	
	if (!dump->Read()) {
		err << "Minidump " << dump->path() << " could not be read" << std::endl;
		return;
	}
	if (minidump_processor->Process(dump.get(), &process_state) !=
		google_breakpad::PROCESS_OK) {
		err << "MinidumpProcessor::Process failed" << std::endl;
		return;
	}
	
	const SystemInfo *system_info = process_state.system_info();
	string cpu = system_info->cpu;

#ifndef _MSC_VER
	// Convert the time to a string
	u_int32_t time_date_stamp = process_state.time_date_stamp();
	struct tm timestruct;
	gmtime_r(reinterpret_cast<time_t*>(&time_date_stamp), &timestruct);
	char timestr[20];
	strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", &timestruct);
	out << "Date: " << timestr << " GMT" << std::endl;
#else
	// TODO ...
#endif

	out << "Operating system: " << system_info->os << " (" << system_info->os_version << ")" << std::endl;
	out << "Architecture: " << cpu << std::endl;
	
	if (process_state.crashed()) {
		out << "Crash reason:  " << process_state.crash_reason() << std::endl;
		out << "Crash address: 0x" << std::hex << process_state.crash_address() << std::dec << std::endl;
	} else {
		out << "No crash" << std::endl;
	}
	
	int requesting_thread = process_state.requesting_thread();
	if (requesting_thread != -1) {
		out << std::endl;
		out << "Thread " << requesting_thread << " (" <<
			(process_state.crashed() ? "crashed" : "requested dump, did not crash") <<
			")" << std::endl;
		PrintThread(process_state.threads()->at(requesting_thread), cpu, out, err);
	}
	
	// Print all of the threads in the dump.
	int thread_count = (int)process_state.threads()->size();
	//const std::vector<google_breakpad::MinidumpMemoryRegion*>
    //*thread_memory_regions = process_state.thread_memory_regions();
	
	for (int thread_index = 0; thread_index < thread_count; ++thread_index) {
		if (thread_index == requesting_thread)
			// Don't print the crash thread again, it was already printed.
			continue;

		out << std::endl;
		out << "Thread " << thread_index << std::endl;
		PrintThread(process_state.threads()->at(thread_index), cpu, out, err);
		
		// Optional
		//google_breakpad::MinidumpMemoryRegion
		//*thread_stack_bytes = thread_memory_regions->at(thread_index);
		
		//thread_stack_bytes->Print();
	}
		
	// Print information about modules
	PrintModules(process_state.modules(), out, err);
}

void MinidumpExtractInfo(const std::string& minidumpfile, std::ostream& out, std::ostream& err) {	
	ProcessSingleReport(minidumpfile, out, err);	
}

#else // NBREAKPAD

void MinidumpExtractInfo(const std::string& minidumpfile, std::ostream& out, std::ostream& err) {
	err << "MinidumpExtractInfo: Google Breakpad support not included in this build" << std::endl;
}

#endif
