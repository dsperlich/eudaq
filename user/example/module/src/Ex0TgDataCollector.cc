#include "eudaq/DataCollector.hh"
#include <mutex>
#include <deque>
#include <map>

class Ex0TgDataCollector:public eudaq::DataCollector{
public:
  Ex0TgDataCollector(const std::string &name,
		   const std::string &rc);
  void DoConnect(eudaq::ConnectionSPC id) override;
  void DoDisconnect(eudaq::ConnectionSPC id) override;
  void DoReceive(eudaq::ConnectionSPC id, eudaq::EventUP ev) override;

  static const uint32_t m_id_factory = eudaq::cstr2hash("Ex0TgDataCollector");
private:
  std::mutex m_mtx_map;
  std::map<eudaq::ConnectionSPC, std::deque<eudaq::EventSPC>> m_conn_evque;
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::DataCollector>::
    Register<Ex0TgDataCollector, const std::string&, const std::string&>
    (Ex0TgDataCollector::m_id_factory);
}

Ex0TgDataCollector::Ex0TgDataCollector(const std::string &name,
				   const std::string &rc):
  DataCollector(name, rc){
}

void Ex0TgDataCollector::DoConnect(eudaq::ConnectionSPC idx){
  std::unique_lock<std::mutex> lk(m_mtx_map);
  m_conn_evque[idx].clear();
}

void Ex0TgDataCollector::DoDisconnect(eudaq::ConnectionSPC idx){
  std::unique_lock<std::mutex> lk(m_mtx_map);
  m_conn_evque.erase(idx);
}

void Ex0TgDataCollector::DoReceive(eudaq::ConnectionSPC idx, eudaq::EventUP ev){  
  std::unique_lock<std::mutex> lk(m_mtx_map);
  eudaq::EventSP evsp = std::move(ev);
  if(!evsp->IsFlagTrigger()){
    EUDAQ_THROW("!evsp->IsFlagTrigger()");
  }
  m_conn_evque[idx].push_back(evsp);

  uint32_t trigger_n = -1;
  for(auto &conn_evque: m_conn_evque){
    if(conn_evque.second.empty())
      return;
    else{
      uint32_t trigger_n_ev = conn_evque.second.front()->GetTriggerN();
      if(trigger_n_ev< trigger_n)
	trigger_n = trigger_n_ev;
    }
  }

  auto ev_sync = eudaq::Event::MakeUnique("Ex0Tg_RAW_DAC");
  ev_sync->SetTriggerN(trigger_n);
  for(auto &conn_evque: m_conn_evque){
    auto &ev_front = conn_evque.second.front(); 
    if(ev_front->GetTriggerN() == trigger_n){
      ev_sync->AddSubEvent(ev_front);
      conn_evque.second.pop_front();
    }
  }
  WriteEvent(std::move(ev_sync));
}