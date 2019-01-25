#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/utils/thread_priority.hpp>
#include <uhd/utils/safe_main.hpp>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/exception.hpp>
#include <boost/program_options.hpp>
#include <boost/format.hpp>
#include <boost/thread.hpp>
#include <boost/thread/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/math/special_functions/round.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <csignal>
#include <complex>
#include <curses.h>
#include <ctime>
#include <chrono>
#include <stdint.h>
#include <time.h>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <mutex> 

std::mutex mtx;           // mutex for critical section

namespace po = boost::program_options;
using namespace std;
using namespace std::chrono;

static bool stop_signal_called = false;
static bool start_signal_called = false;
void sig_int_handler(int){stop_signal_called = true;}

//////////
void send_from_file(
    uhd::usrp::multi_usrp::sptr usrp,
    const std::string &cpu_format,
    const std::string &wire_format,
    const std::string &file,
    size_t samps_per_buff
){

    //create a transmit streamer
    uhd::stream_args_t stream_args(cpu_format, wire_format);
    uhd::tx_streamer::sptr tx_stream = usrp->get_tx_stream(stream_args);

    uhd::tx_metadata_t md;
    md.start_of_burst = false;
    md.end_of_burst = false;
    std::vector<float> buff(samps_per_buff);
    
    std::ifstream infile(file.c_str(), std::ifstream::binary);

    //loop until the entire file has been read

    //while(not md.end_of_burst and not stop_signal_called){
    
    while(not stop_signal_called){

        infile.read((char*)&buff.front(), buff.size()*sizeof(float));
        size_t num_tx_samps = size_t(infile.gcount()/sizeof(float));

        md.end_of_burst = infile.eof();

        tx_stream->send(&buff.front(), num_tx_samps, md);
    }

    infile.close();
    
    /*
    infile.read((char*)&buff.front(), buff.size()*sizeof(float));
    size_t num_tx_samps = size_t(infile.gcount()/sizeof(float));    
    md.end_of_burst = infile.eof();
    tx_stream->send(&buff.front(), num_tx_samps, md);
    */
}

void ReadFromMemory(string msg, string &file, const char* address, size_t size, bool trig_ReadFromMemory)
{   
    sleep(5);
    std::ofstream outfile;
    FILE* pFile;
    outfile.open(file.c_str(), std::ofstream::binary);
    mtx.lock();
    
    //outfile.write((const char*)&buff.at(0), (i-1)*num_rx_samps*sizeof(samp_type)); 
    if (trig_ReadFromMemory == true){
        outfile.write(address, size/2); 
        cout << "task1 says: " << msg;
    }
    mtx.unlock();
    outfile.close();
}

template<typename samp_type> void recv_to_file(
    uhd::usrp::multi_usrp::sptr usrp,
    const std::string &cpu_format,
    const std::string &wire_format,
    const std::string &file,
    const std::string &file_start,
    size_t samps_per_buff,
    unsigned long long num_requested_samples,
    double time_requested = 0.0,
    bool bw_summary = false,
    bool stats = false,
    bool null = false,
    bool enable_size_map = true,
    bool continue_on_bad_packet = false,
    double tx_freq = 0e9,
    double freq = 0e9,
    int64_t start_time = 1485896558700,
    double time_radarrun = 89999000000,//ns
    double rep_time = 0.009,//sec
    double prcs_time = 0
){
    
    unsigned long long num_total_samps = 0;
    uhd::tune_request_t tune_request;
    uhd::tune_request_t tune_request_rx;
    //create a receive streamer
    uhd::stream_args_t stream_args(cpu_format,wire_format);
    uhd::rx_streamer::sptr rx_stream = usrp->get_rx_stream(stream_args);

    //uhd::rx_metadata_t md;
    std::vector<samp_type> buff(samps_per_buff);
    cout<< "sleep 5 seconds" <<endl;//this is necessary
    sleep(5);
    //std::vector<samp_type> buff(num_requested_samples);
    std::ofstream outfile;
    FILE* pFile;
    if (not null)
        outfile.open(file.c_str(), std::ofstream::binary);
        pFile = fopen("file.binary", "wb");
    bool overflow_message = true;

    //setup streaming
    uhd::stream_cmd_t stream_cmd((num_requested_samples == 0)?
        uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS:
        uhd::stream_cmd_t::STREAM_MODE_NUM_SAMPS_AND_DONE//since total_num_samps !=0 so 
    );
    stream_cmd.num_samps = size_t(num_requested_samples);
    stream_cmd.stream_now = false;//C: this is important
    //stream_cmd.time_spec = uhd::time_spec_t();
    boost::system_time start = boost::get_system_time();
    unsigned long long ticks_requested = (long)(time_requested * (double)boost::posix_time::time_duration::ticks_per_second());
    boost::posix_time::time_duration ticks_diff;
    boost::system_time last_update = start;
    unsigned long long last_update_samps = 0;
    
    typedef std::map<size_t,size_t> SizeMap;
    SizeMap mapSizes;
    int num_position = 0;
    char response;
    int i = 0; 
    int ii = 0;
    int i_freq = 0;
    int N = 2500; //target number of measurements
    auto t1 = high_resolution_clock::now();
    auto t2 = high_resolution_clock::now();
    auto tic = high_resolution_clock::now();
    auto toc = high_resolution_clock::now();
    //auto tavg = high_resolution_clock::now();
    auto t_tictoc = std::chrono::duration_cast<std::chrono::nanoseconds>(tic-toc).count();
    auto int_ms1 = std::chrono::duration_cast<std::chrono::nanoseconds>(t1.time_since_epoch());
    auto int_ms2 = std::chrono::duration_cast<std::chrono::nanoseconds>(t2.time_since_epoch());
    long long time_radar;
    long long tavg1=1;
    long long tavg2=1;
    long long d_nanosec;
    time_radar = int_ms2.count()-start_time -60*1e9;
    int wait_time = 0;//sec
    size_t num_rx_samps;

    ///////////////////////////
    //synchronization
    ///////////////////////////
    cout << "Synchronizing with basestation computer, wait " <<wait_time << " sec.\n" <<endl;
    while ((int_ms1.count()-start_time <= wait_time*1e9))
        { 
        t1 = high_resolution_clock::now();
        int_ms1 = std::chrono::duration_cast<std::chrono::nanoseconds>(t1.time_since_epoch());
        //cout << "System Time in nanoseconds is" << int_ms1.count() << scientific << "." << endl;
        //cout << start_time << endl;
        }       
    float t_del = 0;
    double seconds_in_future = 1.1; //sec
    cout << file_start.c_str() << endl;
    std::ifstream ifile(file_start.c_str(), std::ifstream::binary);
    std::ofstream ofile(file_start.c_str(), std::ifstream::binary);
    char buff2;
    bool trig_ReadFromMemory = true;
    boost::thread_group write2HD;
    //use the first line for create the thread
    //write2HD.create_thread(boost::bind(&ReadFromMemory, "Hello Xingjian!!!!!!!!!!!!!!!\n", file, (const char*)&buff.at(0), (i-1)*num_rx_samps*sizeof(samp_type), trig_ReadFromMemory));
    
    //transmit_thread.create_thread(boost::bind(&send_from_file,usrp, "fc32", wirefmt, tx_file, spb));
    //write2HD.create_thread(boost::bind(&outfile.write(),usrp, (const char*)&buff.at(0), (i-1)*num_rx_samps*sizeof(samp_type)));
    //thread write2HD(task1, "Hello\n");
    //outfile.write((const char*)&buff.at(0), (i-1)*num_rx_samps*sizeof(samp_type)); 

    while(not stop_signal_called){   
        ofile.open(file_start.c_str(), std::ifstream::binary);   
        ofile<<"0"<<endl;
        ofile.close();
        ifile.open(file_start.c_str(), std::ifstream::binary);
        ifile.read((char*)&buff2,sizeof(int8_t));
        ifile.close();
        cout << "The flag in run_rx_samples_to_file.dat is ";
        cout << buff2 << endl;     
        i = 0;
        //reset the buffer 
        /*std::ofstream outfile;
        FILE* pFile;
        if (not null)
            outfile.open(file.c_str(), std::ofstream::binary);
        pFile = fopen("file.binary", "wb");
        */
        //Turn on the RF front end    
        //system("./../../../../usbrelay/usbrelay 2>/dev/null");
        //system("./../../../../usbrelay/usbrelay ZIM5B_1=1 ZIM5B_2=1"); 
        
        usrp->set_time_next_pps(uhd::time_spec_t(0.0)); //reset FPGA system time to 0s.
        sleep(1);//let FPGA reset 
        double time_to_receive = usrp->get_time_now().get_full_secs()+usrp->get_time_now().get_frac_secs()+1;
        double time_to_start_whileloop = usrp->get_time_now().get_full_secs();
        //double timeout = std::max(rep_time, seconds_in_future) ; //timeout (delay before transmit + padding)    
        double timeout = 60 ; //in seconds. waiting tram moving out triggering retro reflector switch.    
        //while((time_to_receive-1-rep_time) - time_to_start_whileloop <= time_radarrun){
        //while(i!=10000){
        while(i!=int(time_radarrun/rep_time)){
            if (i == int(time_radarrun/rep_time)-1){
                //i = 0;//uncomment this if want to repeat streaming data to memory.
                }
            if (i == int(time_radarrun/rep_time/2)){
                trig_ReadFromMemory == true;}//if half of the requested data are filled then hard drive starts reading from first part of the allocated memory. 
            //cout << int(time_radarrun/rep_time);
            if(i==1){
                time_to_receive = usrp->get_time_now().get_full_secs()+usrp->get_time_now().get_frac_secs()+1;//this is for fpga code initial condistion inconsistancy at i=2 
                cout <<"this is it"<<endl;
                cout <<time_to_receive<<endl;
            }
            t1 = high_resolution_clock::now();
            int_ms1 = std::chrono::duration_cast<std::chrono::nanoseconds>(t1.time_since_epoch());
            

            //Useful time info
            //cout << int_ms1.count() <<endl;
            cout << time_to_receive-1-time_to_start_whileloop <<endl;
            cout << i << endl;
            cout << time_radarrun <<endl;
            ++i;
            stream_cmd.time_spec = uhd::time_spec_t(time_to_receive);
            rx_stream->issue_stream_cmd(stream_cmd);//issue stream        
            uhd::rx_metadata_t md;
            while((not stop_signal_called) and (num_requested_samples != num_total_samps or num_requested_samples == 0)){
                //num_rx_samps = rx_stream->recv(&buff.at(num_requested_samples*(i-1)), buff.size(), md, timeout, enable_size_map);
                
                //size_t  num_rx_samps = rx_stream->recv(&buff.front(), buff.size(), md, timeout, enable_size_map);
                //num_rx_samps = rx_stream->recv(&buff.at(samps_per_buff*(ii)), samps_per_buff, md, timeout, enable_size_map);
                mtx.lock();
                
                num_rx_samps = rx_stream->recv(&buff.at(num_requested_samples*(i-1)), buff.size(), md, timeout, enable_size_map);
                mtx.unlock();
                if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE){
                    std::string error = str(boost::format("Receiver error: %s") % md.strerror());
                    throw std::runtime_error(error);
                    /*
                    //if (continue_on_bad_packet){
                    if (1){
                        std::cerr << error << std::endl;
                        continue;
                    }
                    else
                        throw std::runtime_error(error);
                        */
                }
                
                num_total_samps += num_rx_samps;
                ii += 1;
                cout << num_total_samps << endl;
                //cout << buff.size()/10 << endl;
                //cout << samps_per_buff << endl;
                outfile.write((const char*)&buff.at(num_requested_samples*(i-1)), num_rx_samps*sizeof(samp_type));  //use this line to write to hardware    
                //usleep(100);
            }
            //outfile.write((const char*)&buff.at(0), num_total_samps*sizeof(samp_type));    
            ii = 0;
            num_total_samps = 0;
            std::cout << boost::format(
            " Received packet: %u samples, %u full secs, %f frac secs"
            ) % num_rx_samps % md.time_spec.get_full_secs() % md.time_spec.get_frac_secs() << std::endl;
            std::cout << boost::format(
                " Stream time was: %u full secs, %f frac secs"
            ) % stream_cmd.time_spec.get_full_secs() % stream_cmd.time_spec.get_frac_secs() << std::endl;
            std::cout << boost::format(
            " Difference between stream time and first packet: %f us"
            ) % ((md.time_spec-stream_cmd.time_spec).get_real_secs()*1e6) << std::endl;  
            
            time_to_receive = md.time_spec.get_full_secs()+md.time_spec.get_frac_secs();  
            time_to_receive += rep_time;
            
        }//while loop end
        
        //if (outfile.is_open()){
        //    outfile.write((const char*)&buff.at(0), (i-1)*num_rx_samps*sizeof(samp_type));  //use this line to write to hardware
        //} 
        
        //turn off the rf frontend
        //system("./../../../../usbrelay/usbrelay ZIM5B_1=0 ZIM5B_2=0");

        //stream_cmd.stream_mode = uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS;
        //rx_stream->issue_stream_cmd(stream_cmd);

        if (outfile.is_open())
            outfile.close();
            fclose(pFile);

        if (stats) {
            std::cout << std::endl;

            double t = (double)ticks_diff.ticks() / (double)boost::posix_time::time_duration::ticks_per_second();
            std::cout << boost::format("Received %d samples in %f seconds") % num_total_samps % t << std::endl;
            double r = (double)num_total_samps / t;
            std::cout << boost::format("%f Msps") % (r/1e6) << std::endl;

            if (enable_size_map) {
                std::cout << std::endl;
                std::cout << "Packet size map (bytes: count)" << std::endl;
                for (SizeMap::iterator it = mapSizes.begin(); it != mapSizes.end(); it++)
                    std::cout << it->first << ":\t" << it->second << std::endl;
            }
        }
        //////////////////////////////
        //Post Process
        //////////////////////////////
        /*system("python  test_avg2.py");
        //system("python call_pixplot.py");
        sleep(prcs_time);
        //sleep(600);
        ofile.open(file_start.c_str(), std::ifstream::binary);
        ofile<<"1"<<endl;
        ofile.close();
        ifile.open(file_start.c_str(), std::ifstream::binary);
        ifile.read((char*)&buff2,sizeof(int8_t));
        ifile.close();
        cout << "The flag in run_rx_samples_to_file.dat is ";
        cout << buff2 << endl;
        std::cout << std::endl << "Done!" << std::endl << std::endl;
        */
        //boost::this_thread::sleep(boost::posix_time::milliseconds(10));
        //break;
    }
}

typedef boost::function<uhd::sensor_value_t (const std::string&)> get_sensor_fn_t;

bool check_locked_sensor(std::vector<std::string> sensor_names, const char* sensor_name, get_sensor_fn_t get_sensor_fn, double setup_time){
    if (std::find(sensor_names.begin(), sensor_names.end(), sensor_name) == sensor_names.end())
        return false;

    boost::system_time start = boost::get_system_time();
    boost::system_time first_lock_time;

    std::cout << boost::format("Waiting for \"%s\": ") % sensor_name;
    //std::cout.flush();

    while (true) {
        if ((not first_lock_time.is_not_a_date_time()) and
                (boost::get_system_time() > (first_lock_time + boost::posix_time::seconds(setup_time))))
        {
            std::cout << " locked." << std::endl;
            break;
        }
        if (get_sensor_fn(sensor_name).to_bool()){
            if (first_lock_time.is_not_a_date_time())
                first_lock_time = boost::get_system_time();
            std::cout << "+";
            std::cout.flush();
        }
        else {
            first_lock_time = boost::system_time(); //reset to 'not a date time'

            if (boost::get_system_time() > (start + boost::posix_time::seconds(setup_time))){
                std::cout << std::endl;
                throw std::runtime_error(str(boost::format("timed out waiting for consecutive locks on sensor \"%s\"") % sensor_name));
            }
            std::cout << "_";
            std::cout.flush();
        }
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
    }
    std::cout << std::endl;
    return true;
}

int UHD_SAFE_MAIN(int argc, char *argv[]){
    uhd::set_thread_priority_safe();

    //variables to be set by po
    std::string rx_channels, tx_channels, args, tx_args, file, tx_file, file_start, type, ant, subdev, tx_subdev, ref, wirefmt;
    size_t total_num_samps, spb_tx, spb;
    double rate, tx_rate, freq, gain, bw, total_time, setup_time, delay, tx_freq, tx_gain, tx_bw, time_radarrun, rep_time, prcs_time;
    int64_t start_time;    
    //setup the program options
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "help message")
        ("prcs_time", po::value<double>(&prcs_time)->default_value(0), "process time, waiting for data process after recoording [sec]")
        ("rep_time", po::value<double>(&rep_time)->default_value(0.009), "repeat time for rx streaming [sec]")
        ("time_radarrun", po::value<double>(&time_radarrun)->default_value(89.999), "Radar operation time in second using computer clock")
        ("start_time", po::value<int64_t>(&start_time)->default_value(1485896558703), "present time in milliseconds")
        ("args", po::value<std::string>(&args)->default_value(""), "multi uhd device address args")
        ("tx-args", po::value<std::string>(&tx_args)->default_value(""), "uhd transmit device address args")
        ("file", po::value<std::string>(&file)->default_value("usrp_samples.dat"), "name of the file to write binary samples to")
        ("tx_file", po::value<std::string>(&tx_file)->default_value("usrp_samples.dat"), "name of the file to read binary samples from for tx")
        ("file_start", po::value<std::string>(&file_start)->default_value("run_rx_samples_to_file.dat"), "name of the file to start rx from python")
        ("type", po::value<std::string>(&type)->default_value("short"), "sample type: double, float, or short")
        ("nsamps", po::value<size_t>(&total_num_samps)->default_value(0), "total number of samples to receive")
        ("duration", po::value<double>(&total_time)->default_value(0), "total number of seconds to receive")
        ("time", po::value<double>(&total_time), "(DEPRECATED) will go away soon! Use --duration instead")
        ("spb", po::value<size_t>(&spb)->default_value(10000), "samples per buffer")
        ("spb_tx", po::value<size_t>(&spb_tx)->default_value(10000), "TX samples per buffer")
        ("rate", po::value<double>(&rate)->default_value(1e6), "rate of incoming samples")
        ("tx_rate", po::value<double>(&tx_rate)->default_value(1e6), "Transmit rate of incoming samples")
        ("freq", po::value<double>(&freq)->default_value(0.0), "RF center frequency in Hz")
        ("tx_freq", po::value<double>(&tx_freq), "Transmit RF center frequency in Hz")
        ("gain", po::value<double>(&gain), "gain for the RF chain")
        ("tx_gain", po::value<double>(&tx_gain), "Transmit gain for the RF chain")
        ("ant", po::value<std::string>(&ant), "antenna selection")
        ("subdev", po::value<std::string>(&subdev), "subdevice specification")
        ("tx-subdev", po::value<std::string>(&tx_subdev), "transmit subdevice specification")
        ("bw", po::value<double>(&bw), "analog frontend filter bandwidth in Hz")
        ("tx_bw", po::value<double>(&tx_bw), "Transmit analog frontend filter bandwidth in Hz")
        ("ref", po::value<std::string>(&ref)->default_value("internal"), "reference source (internal, external, mimo)")
        ("wirefmt", po::value<std::string>(&wirefmt)->default_value("fc32"), "wire format (sc8 or sc16)")
        ("setup", po::value<double>(&setup_time)->default_value(1.0), "seconds of setup time")
        ("progress", "periodically display short-term bandwidth")
        ("stats", "show average bandwidth on exit")
        ("sizemap", "track packet size and display breakdown on exit")
        ("null", "run without writing to file")
        ("continue", "don't abort on a bad packet")
        ("skip-lo", "skip checking LO lock status")
        ("int-n", "tune USRP with integer-N tuning")
        ("delay", po::value<double>(&delay)->default_value(0.0), "specify a delay between repeated transmission of file")
        ("repeat", "repeatedly transmit file")        
        ("tx-channels", po::value<std::string>(&tx_channels)->default_value("0"), "which TX channel(s) to use (specify \"0\", \"1\", \"0,1\", etc)")
        ("rx-channels", po::value<std::string>(&rx_channels)->default_value("0"), "which RX channel(s) to use (specify \"0\", \"1\", \"0,1\", etc)")
    ;
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    //print the help message
    if (vm.count("help")) {
        std::cout << boost::format("UHD RX samples to file %s") % desc << std::endl;
        std::cout
            << std::endl
            << "This application streams data from a single channel of a USRP device to a file.\n"
            << std::endl;
        return ~0;
    }
    
    bool repeat = vm.count("repeat") > 0;
    bool bw_summary = vm.count("progress") > 0;
    bool stats = vm.count("stats") > 0;
    bool null = vm.count("null") > 0;
    bool enable_size_map = vm.count("sizemap") > 0;
    bool continue_on_bad_packet = vm.count("continue") > 0;

    if (enable_size_map)
        std::cout << "Packet size tracking enabled - will only recv one packet at a time!" << std::endl;

    //create a usrp device
    std::cout << std::endl;
    std::cout << boost::format("Creating the transmit usrp device with: %s...") % tx_args << std::endl;
    uhd::usrp::multi_usrp::sptr tx_usrp = uhd::usrp::multi_usrp::make(tx_args);
    std::cout << std::endl;
    std::cout << boost::format("Creating the usrp device with: %s...") % args << std::endl;
    uhd::usrp::multi_usrp::sptr usrp = uhd::usrp::multi_usrp::make(args);

    //Lock mboard clocks
    usrp->set_clock_source(ref);
    tx_usrp->set_clock_source(ref);
    //detect which channels to use
    std::vector<std::string> tx_channel_strings;
    std::vector<size_t> tx_channel_nums;
    boost::split(tx_channel_strings, tx_channels, boost::is_any_of("\"',"));
    for(size_t ch = 0; ch < tx_channel_strings.size(); ch++){
        size_t chan = boost::lexical_cast<int>(tx_channel_strings[ch]);
        if(chan >= tx_usrp->get_tx_num_channels()){
            throw std::runtime_error("Invalid TX channel(s) specified.");
        }
        else tx_channel_nums.push_back(boost::lexical_cast<int>(tx_channel_strings[ch]));
    }
   std::vector<std::string> rx_channel_strings;
    std::vector<size_t> rx_channel_nums;
    boost::split(rx_channel_strings, rx_channels, boost::is_any_of("\"',"));
    for(size_t ch = 0; ch < rx_channel_strings.size(); ch++){
        size_t chan = boost::lexical_cast<int>(rx_channel_strings[ch]);
        if(chan >= usrp->get_rx_num_channels()){
            throw std::runtime_error("Invalid RX channel(s) specified.");
        }
        else rx_channel_nums.push_back(boost::lexical_cast<int>(rx_channel_strings[ch]));
    }

    //always select the subdevice first, the channel mapping affects the other settings
    if (vm.count("subdev")) usrp->set_rx_subdev_spec(subdev);
    if (vm.count("tx-subdev")) tx_usrp->set_tx_subdev_spec(tx_subdev);
    std::cout << boost::format("Using RX Device: %s") % usrp->get_pp_string() << std::endl;
    std::cout << boost::format("Using TX Device: %s") % tx_usrp->get_pp_string() << std::endl;
    


    //Set Master Clock Rate
    usrp->set_master_clock_rate(56e6, 0);// C: this is important to set RF frontend bandwidth to 56e6Hz which can be independent from data rate.
//////////////////////////////////////////
    //Transmit

    //set the sample rate
    if (not vm.count("tx_rate")){
        std::cerr << "Please specify the sample rate with --rate" << std::endl;
        return ~0;
    }
    std::cout << boost::format("Setting TX Rate: %f Msps...") % (tx_rate/1e6) << std::endl;
    usrp->set_tx_rate(tx_rate);
    std::cout << boost::format("Actual TX Rate: %f Msps...") % (usrp->get_tx_rate()/1e6) << std::endl << std::endl;

    //set the center frequency
    if (not vm.count("tx_freq")){
        std::cerr << "Please specify the center frequency with --tx_freq" << std::endl;
        return ~0;
    }
    std::cout << boost::format("Setting TX Freq: %f MHz...") % (tx_freq/1e6) << std::endl;
    uhd::tune_request_t tune_request;
    tune_request = uhd::tune_request_t(tx_freq);
    if(vm.count("int-n")) tune_request.args = uhd::device_addr_t("mode_n=integer");
    usrp->set_tx_freq(tune_request);
    std::cout << boost::format("Actual TX Freq: %f MHz...") % (usrp->get_tx_freq()/1e6) << std::endl << std::endl;
    
    //set the rf gain
    if (vm.count("tx_gain")){
        std::cout << boost::format("Setting TX Gain: %f dB...") % tx_gain << std::endl;
        usrp->set_tx_gain(tx_gain);
        std::cout << boost::format("Actual TX Gain: %f dB...") % usrp->get_tx_gain() << std::endl << std::endl;
    }

    //set the analog frontend filter bandwidth
    if (vm.count("tx_bw")){
        std::cout << boost::format("Setting TX Bandwidth: %f MHz...") % (tx_bw/1e6) << std::endl;
        usrp->set_tx_bandwidth(tx_bw);
        std::cout << boost::format("Actual TX Bandwidth: %f MHz...") % (usrp->get_tx_bandwidth()/1e6) << std::endl << std::endl;
    }
    /////////////////////////////////////
    //Transmit
    /////////////////////////////////////
    
    //create a transmit streamer
    //linearly map channels (index0 = channel0, index1 = channel1, ...)
    uhd::stream_args_t stream_args("fc32", "sc16");
    stream_args.channels = tx_channel_nums;
    uhd::tx_streamer::sptr tx_stream = tx_usrp->get_tx_stream(stream_args);

    //allocate a buffer which we re-use for each channel
    if (spb == 0) spb = tx_stream->get_max_num_samps()*10;
    std::vector<std::complex<float> > buff(spb_tx);
    int num_channels = tx_channel_nums.size();

    //setup the metadata flags
    uhd::tx_metadata_t md;
    md.start_of_burst = true;
    md.end_of_burst   = false;
    md.has_time_spec  = true;
    md.time_spec = uhd::time_spec_t(0.1); //give us 0.1 seconds to fill the tx buffers

    //Check Ref and LO Lock detect
    std::vector<std::string> tx_sensor_names, rx_sensor_names;
    tx_sensor_names = tx_usrp->get_tx_sensor_names(0);
    if (std::find(tx_sensor_names.begin(), tx_sensor_names.end(), "lo_locked") != tx_sensor_names.end()) {
        uhd::sensor_value_t lo_locked = tx_usrp->get_tx_sensor("lo_locked",0);
        std::cout << boost::format("Checking TX: %s ...") % lo_locked.to_pp_string() << std::endl;
        UHD_ASSERT_THROW(lo_locked.to_bool());

    
    //send from file
    /*int k;
    do{
    send_from_file(usrp, "fc32", wirefmt, tx_file, spb);
    if(repeat and delay != 0.0) boost::this_thread::sleep(boost::posix_time::milliseconds(delay));
    k=k+1;
    std::cout << k << std::endl;
    } while(repeat and not stop_signal_called and k!=50);
    */

    //start trans`mit worker thread
    boost::thread_group transmit_thread;
    transmit_thread.create_thread(boost::bind(&send_from_file,usrp, "fc32", wirefmt, tx_file, spb));
    // start trig_rec
    boost::thread_group trig_rec_thread;

    ///////////////////////////////////////////////
    //Receive
    //////////fz/////////////////////////////////////
    //set the sample rate
    if (rate <= 0.0){
        std::cerr << "Please specify a valid sample rate" << std::endl;
        return ~0;
    }
    std::cout << boost::format("Setting RX Rate: %f Msps...") % (rate/1e6) << std::endl;
    usrp->set_rx_rate(rate);
    std::cout << boost::format("Actual RX Rate: %f Msps...") % (usrp->get_rx_rate()/1e6) << std::endl << std::endl;

    //set the center frequency
    if (vm.count("freq")) { //with default of 0.0 this will always be true
        std::cout << boost::format("Setting RX Freq: %f MHz...") % (freq/1e6) << std::endl;
        uhd::tune_request_t tune_request(freq);
        if(vm.count("int-n")) tune_request.args = uhd::device_addr_t("mode_n=integer");
        usrp->set_rx_freq(tune_request);
        std::cout << boost::format("Actual RX Freq: %f MHz...") % (usrp->get_rx_freq()/1e6) << std::endl << std::endl;
    }

    //set the rf gain
    if (vm.count("gain")) {
        std::cout << boost::format("Setting RX Gain: %f dB...") % gain << std::endl;
        usrp->set_rx_gain(gain);
        std::cout << boost::format("Actual RX Gain: %f dB...") % usrp->get_rx_gain() << std::endl << std::endl;
    }

    //set the IF filter bandwidth
    if (vm.count("bw")) {
        std::cout << boost::format("Setting RX Bandwidth: %f MHz...") % (bw/1e6) << std::endl;
        usrp->set_rx_bandwidth(bw);
        std::cout << boost::format("Actual RX Bandwidth: %f MHz...") % (usrp->get_rx_bandwidth()/1e6) << std::endl << std::endl;
    }

    //set the antenna
    if (vm.count("ant")) usrp->set_rx_antenna(ant);

    boost::this_thread::sleep(boost::posix_time::seconds(setup_time)); //allow for some setup time

    //check Ref and LO Lock detect
    if (not vm.count("skip-lo")){
        check_locked_sensor(usrp->get_rx_sensor_names(0), "lo_locked", boost::bind(&uhd::usrp::multi_usrp::get_rx_sensor, usrp, _1, 0), setup_time);
        if (ref == "mimo")
            check_locked_sensor(usrp->get_mboard_sensor_names(0), "mimo_locked", boost::bind(&uhd::usrp::multi_usrp::get_mboard_sensor, usrp, _1, 0), setup_time);
        if (ref == "external")
            check_locked_sensor(usrp->get_mboard_sensor_names(0), "ref_locked", boost::bind(&uhd::usrp::multi_usrp::get_mboard_sensor, usrp, _1, 0), setup_time);
    }

    if (total_num_samps == 0){
        std::signal(SIGINT, &sig_int_handler);
        std::cout << "Press Ctrl + C to stop streaming..." << std::endl;
    }

    //set sigint if user wants to receive
    if(repeat){
        std::signal(SIGINT, &sig_int_handler);
        std::cout << "Press Ctrl + C to stop streaming..." << std::endl;
    }


#define recv_to_file_args(format) \
    (usrp, format, wirefmt, file, file_start, spb, total_num_samps, total_time, bw_summary, stats, null, enable_size_map, continue_on_bad_packet, tx_freq,freq, start_time, time_radarrun,rep_time,prcs_time)
    //recv to file
    if (type == "double") recv_to_file<std::complex<double> >recv_to_file_args("fc64");
    else if (type == "float") recv_to_file<std::complex<float> >recv_to_file_args("fc32");
    else if (type == "short") recv_to_file<std::complex<short> >recv_to_file_args("sc16");
    else throw std::runtime_error("Unknown type " + type);
    //start_signal_called = false;

    //clean up transmit worker
    stop_signal_called = true;//There is another part of code in fpga to repeat the transmit waveform
    transmit_thread.join_all();//This will stop the tx function
    return EXIT_SUCCESS;
}}
    