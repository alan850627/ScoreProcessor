#ifndef IMAGE_PROCESS_H
#define IMAGE_PROCESS_H
#include "CImg.h"
#include <vector>
#include <memory>
#include <utility>
#include <string>
#include "lib\threadpool\ThreadPool.h"
#include "lib\exstring\exstring.h"
#include <stdexcept>
#include <regex>
#include <functional>
#include "lib\exstring\exmath.h"
namespace ScoreProcessor {
	template<typename T=unsigned char>
	class ImageProcess {
	public:
		typedef cimg_library::CImg<T> Img;
		virtual ~ImageProcess()
		{};
		virtual void process(Img&)=0;
	};
	/*
	template<typename T=unsigned char>
	class SaveProcess:public ImageProcess<T> {
		char* filename;
	public:
		template<typename String>
		SaveProcess(String& path)
		{
			filename=new char[path.size()+1];
			size_t i;
			for(i=0;i<path.size();++i)
			{
				filename[i]=path[i];
			}
			filename[i]='\0';
		}
		SaveProcess(char const* path)
		{
			size_t s=strlen(path);
			filename=new char[s+1];
			filename[s]='\0';
			--s;
			do
			{
				filename[s]=path[s];
				--s;
			} while(s>0);
		}
		void process(Img& img) override
		{
			img.save(filename);
		}
		~SaveProcess()
		{
			delete[] filename;
		}
	};
	*/
	class Log {
	public:
		virtual ~Log()=default;
		virtual void log(char const*)=0;
	};
	class CoutLog:public Log,private exlib::OstreamLogger {
	public:
		CoutLog():exlib::OstreamLogger(std::cout)
		{}
		void log(char const* out) override
		{
			exlib::OstreamLogger::log(out);
		}
	};
	class SaveRules {
	private:
		template<typename T>
		friend class ProcessList;

		enum template_symbol:unsigned int {
			i,x=10,f,p,c
		};
		union part {
			exlib::weak_string fixed;
			struct {
				size_t is_fixed;//relies on weak_string having format [data_pointer,size]
				size_t tmplt;
			};
			part(part&& o):is_fixed(o.is_fixed),tmplt(o.tmplt)
			{
				o.is_fixed=0;
			}
			part(unsigned int tmplt):is_fixed(0),tmplt(tmplt)
			{}
			part(exlib::string& str):
				fixed(str.data(),str.size())
			{
				str.release();
			}
			~part()
			{
				delete[] fixed.data();
			}
		};
		std::vector<part> parts;
	public:
		template<typename String>
		exlib::string make_filename(String const& input,unsigned int index=0) const;

		exlib::string make_filename(char const* input,unsigned int index=0) const;
		/*
		%f to match filename
		%x to match extension
		%p to match path
		%c to match entirety of input
		%0 for index, with any number 0-9 for amount of padding
		empty string, equivalent to %f alone
		*/
		template<typename String>
		SaveRules(String const& tmplt);
		SaveRules(char const* tmplt);
		SaveRules();

		template<typename String>
		void assign(String const& tmplt);
		void assign(char const* tmplt);

		bool empty() const;
	};

	template<typename T=unsigned char>
	class ProcessList:public std::vector<std::unique_ptr<ImageProcess<T>>> {
	private:
		class ProcessTaskImg:public exlib::ThreadTask {
		private:
			cimg_library::CImg<T>* pimg;
			ProcessList<T> const* pparent;
			SaveRules const* output;
			unsigned int index;
		public:
			ProcessTaskImg(cimg_library::CImg<T>* pimg,ProcessList<T> const* pparent,SaveRules const* output,unsigned int index):
				pimg(pimg),pparent(pparent),output(output),index(index)
			{}
			void execute() override
			{
				pparent->process(*pimg,output,index);
			}
		};
		class ProcessTaskFName:public exlib::ThreadTask {
		private:
			char const* fname;
			ProcessList<T> const* pparent;
			SaveRules const* output;
			unsigned int index;
		public:
			ProcessTaskFName(char const* fname,ProcessList<T> const* pparent,SaveRules const* output,unsigned int index):
				fname(fname),pparent(pparent),output(output),index(index)
			{}
			void execute() override
			{
				try
				{
					pparent->process(fname,output,index);
				}
				catch(std::exception const& ex)
				{
					std::cout<<ex.what()<<'\n';//REPLACE WITH LOGGER
				}
			}
		};
	public:
		template<typename U,typename... Args>
		void add_process(Args&&... args);

		void process(cimg_library::CImg<T>& img) const;
		void process(cimg_library::CImg<T>& img,char const* output) const;
		void process(cimg_library::CImg<T>& img,SaveRules const* psr,unsigned int index=0) const;

		void process(char const* filename) const;
		void process(char const* filename,char const* output) const;
		void process(char const* filename,SaveRules const* psr,unsigned int index=0) const;

		/*
			Pass nullptr to psr if you do not want the imgs to be saved.
		*/
		void process(std::vector<cimg_library::CImg<T>>& files,
			SaveRules const* psr,
			unsigned int num_threads=std::thread::hardware_concurrency()) const;

		template<typename String>
		void process(std::vector<String> const& filenames,
			SaveRules const* psr,
			unsigned int num_threads=std::thread::hardware_concurrency()) const;

		void process(std::vector<char*> const& filenames,
			SaveRules const* psr,
			unsigned int num_threads=std::thread::hardware_concurrency()) const;
	};
	typedef ProcessList<unsigned char> IPList;

	template<typename T>
	template<typename U,typename... Args>
	void ProcessList<T>::add_process(Args&&... args)
	{
		emplace_back(std::make_unique<U>(std::forward<Args>(args)...));
	}

	template<typename T>
	void ProcessList<T>::process(cimg_library::CImg<T>& img) const
	{
		process(img,nullptr);
	}

	template<typename T>
	void ProcessList<T>::process(cimg_library::CImg<T>& img,char const* output) const
	{
		for(auto& pprocess:*this)
		{
			pprocess->process(img);
		}
		if(output!=nullptr)
		{
			img.save(output);
		}
	}

	template<typename T>
	void ProcessList<T>::process(cimg_library::CImg<T>& img,SaveRules const* psr,unsigned int index=0) const
	{
		if(psr==nullptr)
		{
			process(img);
		}
		else
		{
			auto outname=psr->make_filename("",index);
			process(img,outname.c_str());
		}
	}

	template<typename T>
	void ProcessList<T>::process(char const* fname) const
	{
		process(fname,fname);
	}

	template<typename T>
	void ProcessList<T>::process(char const* fname,char const* output) const
	{
		process(cimg_library::CImg<T>(fname),output);
	}

	template<typename T>
	void ProcessList<T>::process(char const* filename,SaveRules const* psr,unsigned int index) const
	{
		if(psr==nullptr)
		{
			process(filename);
		}
		auto output=psr->make_filename(filename,index);
		process(filename,output.c_str());
	}

	template<typename T>
	void ProcessList<T>::process(
		std::vector<char*> const& imgs,
		SaveRules const* psr,
		unsigned int num_threads) const
	{
		exlib::ThreadPool tp(num_threads);
		for(size_t i=0;i<imgs.size();++i)
		{
			tp.add_task<typename ProcessList<T>::ProcessTaskFName>(imgs[i],this,psr,i+1);
		}
		tp.start();
	}

	template<typename T>
	template<typename String>
	void ProcessList<T>::process(
		std::vector<String> const& imgs,
		SaveRules const* psr,
		unsigned int num_threads) const
	{
		exlib::ThreadPool tp(num_threads);
		for(size_t i=0;i<imgs.size();++i)
		{
			tp.add_task<typename ProcessList<T>::ProcessTaskFName>(imgs[i].c_str(),this,psr,i+1);
		}
		tp.start();
	}

	template<typename T>
	void ProcessList<T>::process(
		std::vector<cimg_library::CImg<T>>& imgs,
		SaveRules const* psr,
		unsigned int num_threads) const
	{
		exlib::ThreadPool tp(num_threads);
		for(size_t i=0;i<imgs.size();++i)
		{
			tp.add_task<typename ProcessList<T>::ProcessTaskImg>(imgs[i],this,psr,i+1);
		}
		tp.start();
	}

	template<typename String>
	SaveRules::SaveRules(String const& tmplt)
	{
		assign(tmplt.c_str());
	}

	SaveRules::SaveRules(char const* tmplt)
	{
		assign(tmplt);
	}

	SaveRules::SaveRules()
	{}

	template<typename String>
	void SaveRules::assign(String const& tmplt)
	{
		assign(tmplt.c_str());
	}

	inline void SaveRules::assign(char const* tmplt)
	{
		parts.clear();
		size_t i=0;
		bool found=false;
		exlib::string str(20);
		auto put_string=[&]()
		{
			parts.emplace_back(str);
			str.reserve(20);
		};
		while(tmplt[i]!=0)
		{
			if(found)
			{
				found=false;
				char letter=tmplt[i];
				if(letter>='0'&&letter<='9')
				{
					put_string();
					parts.emplace_back(letter-'0');
				}
				else
				{
					switch(letter)
					{
						case 'x':
							put_string();
							parts.emplace_back(template_symbol::x);
							break;
						case 'f':
							put_string();
							parts.emplace_back(template_symbol::f);
							break;
						case 'p':
							put_string();
							parts.emplace_back(template_symbol::p);
							break;
						case 'c':
							put_string();
							parts.emplace_back(template_symbol::c);
							break;
						case '%':
							str.push_back('%');
							break;
						default:
							throw std::invalid_argument("Invalid escape character");
					}
				}
			}
			else
			{
				if(tmplt[i]=='%')
				{
					found=true;
				}
				else
				{
					str.push_back(tmplt[i]);
				}
			}
			++i;
		}
		put_string();
	}

	template<typename String>
	exlib::string SaveRules::make_filename(String const& input,unsigned int index) const
	{
		exlib::string out(30L);
		exlib::weak_string ext(nullptr,0);
		exlib::weak_string filename(nullptr,0);
		exlib::weak_string path(nullptr,0);
		auto find_filename=[](String const& in)
		{
			char* it=const_cast<char*>(&(*(in.cend()-1)));
			while(1)
			{
				if(*it=='\\'||*it=='/')
				{
					return it;
				}
				if(it==(&(*in.cbegin())))
				{
					return it;
				}
				--it;
			}
		};
		auto check_ext=[&]()
		{
			if(ext.data()==nullptr)
			{
				ext=exlib::weak_string(const_cast<char*>(::cimg_library::cimg::split_filename(input.c_str())));
			}
		};
		auto check_filename=[&]()
		{
			if(filename.data()==nullptr)
			{
				check_ext();
				char* fstart=find_filename(input);
				size_t fsize=ext.data()-fstart;
				if(ext.data()!=(&(*input.cend())))
				{
					--fsize;
				}
				filename=exlib::weak_string(fstart,fsize);
			}
		};
		auto check_path=[&]()
		{
			if(path.data()==nullptr)
			{
				check_filename();
				size_t psize=filename.data()-input.data();
				path=exlib::weak_string(const_cast<char*>(input.data()),psize);
			}
		};
		for(auto const& p:parts)
		{
			if(p.is_fixed)
			{
				out+=p.fixed;
			}
			else
			{
				if(p.tmplt<10)
				{
					out+=exlib::front_padded_string(std::to_string(index),p.tmplt,'0');
				}
				else
				{
					switch(p.tmplt)
					{
						case SaveRules::template_symbol::x:
							check_ext();
							out+=ext;
							break;
						case SaveRules::template_symbol::f:
							check_filename();
							out+=filename;
							break;
						case SaveRules::template_symbol::p:
							check_path();
							out+=path;
							break;
						case SaveRules::template_symbol::c:
							out+=input;
							break;
					}
				}
			}
		}
		return out;
	}

	inline exlib::string SaveRules::make_filename(char const* input,unsigned int index) const
	{
		return make_filename(exlib::weak_string(const_cast<char*>(input)),index);
	}

	inline bool SaveRules::empty() const
	{
		return parts.empty();
	}
}
#endif
