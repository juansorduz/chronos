/**
 * @file chronos_gr_connection.cpp.
 *
 * Copyright (C) Metaswitch Networks 2016
 * If license terms are provided to you in a COPYING file in the root directory
 * of the source code repository by which you are accessing this code, then
 * the license outlined in that COPYING file applies to your use.
 * Otherwise no rights are granted except for those provided to you by
 * Metaswitch Networks in a separate written agreement.
 */

#include "log.h"
#include "sasevent.h"
#include "chronos_gr_connection.h"

ChronosGRConnection::ChronosGRConnection(const std::string& remote_site,
                                         HttpResolver* resolver,
                                         BaseCommunicationMonitor* comm_monitor) :
  _site_name(remote_site),
  _http_client(new HttpClient(false,
                              resolver,
                              nullptr,
                              nullptr,
                              SASEvent::HttpLogLevel::NONE,
                              nullptr,
                              false,
                              true)),
  _http_conn(new HttpConnection(remote_site,
                                _http_client)),
  _comm_monitor(comm_monitor)
{
}

ChronosGRConnection::~ChronosGRConnection()
{
  delete _http_conn; _http_conn = nullptr;
  delete _http_client; _http_client = nullptr;
}

void ChronosGRConnection::send_put(std::string url,
                                   std::string body)
{
  HTTPCode rc = _http->send_put(url, body, 0);

  if (rc != HTTP_OK)
  {
    // LCOV_EXCL_START - No value in testing this log in UT
    TRC_ERROR("Unable to send replication to a remote site (%s)",
              _site_name.c_str());

    if (_comm_monitor)
    {
      _comm_monitor->inform_failure();
    }
    // LCOV_EXCL_STOP
  }
  else
  {
    if (_comm_monitor)
    {
      _comm_monitor->inform_success();
    }
  }
}
