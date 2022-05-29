#ifndef H_39B56032251A44728943666BD008D047
#define H_39B56032251A44728943666BD008D047

#include <cstring>
#include <vector>
#include <utility>
#include <string_view>
#include <optional>

namespace ws28 {
	class Client;
	
	class RequestHeaders {
	public:
		void Set(std::string_view key, std::string_view value){
			m_Headers.push_back({ key, value });
		}
		
		template<typename F>
		void ForEachValueOf(std::string_view key, const F &f) const {
			for(auto &p : m_Headers){
				if(p.first == key) f(p.second);
			}
		}
		
		std::optional<std::string_view> Get(std::string_view key) const {
			for(auto &p : m_Headers){
				if(p.first == key) return p.second;
			}
			
			return std::nullopt;
		}
		
		template<typename F>
		void ForEach(const F &f) const {
			for(auto &p : m_Headers){
				f(p.first, p.second);
			}
		}
		
	private:
		std::vector<std::pair<std::string_view, std::string_view>> m_Headers;
		
		friend class Client;
		friend class Server;
	};
	
}

#endif
