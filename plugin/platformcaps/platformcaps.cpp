/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "platformcaps.h"

#include "platformcapsdata.h"

#include <regex>

namespace WPEFramework {
namespace Plugin {

bool PlatformCaps::Load(PluginHost::IShell* service, const string &query, Exchange::ISystemServices::PlatformConfig& platformConfig) {
  bool result = true;

  Reset();

  std::smatch m;
  bool matched = std::regex_search(query, m,
      std::regex("^(AccountInfo|DeviceInfo)(\\.(\\w*)){0,1}"));

  if (query.empty() || matched) {
    if (query.empty() || (m[1] == _T("AccountInfo"))) {
      if (!accountInfo.Load(service, m.size() > 3 ? m[3] : string())) {
        result = false;
      }
      Add(_T("AccountInfo"), &accountInfo);
      platformConfig.accountInfo.accountId = accountInfo.accountId.Value();
      platformConfig.accountInfo.x1DeviceId = accountInfo.x1DeviceId.Value();
      platformConfig.accountInfo.xCALSessionTokenAvailable = accountInfo.XCALSessionTokenAvailable.Value();
      platformConfig.accountInfo.experience = accountInfo.experience.Value();
      platformConfig.accountInfo.deviceMACAddress = accountInfo.deviceMACAddress.Value();
      platformConfig.accountInfo.firmwareUpdateDisabled = accountInfo.firmwareUpdateDisabled.Value();
    }

    if (query.empty() || (m[1] == _T("DeviceInfo"))) {
      if (!deviceInfo.Load(service, m.size() > 3 ? m[3] : string())) {
        result = false;
      }
      Add(_T("DeviceInfo"), &deviceInfo);

      // Convert quirks array to comma-separated string
      auto& quirksArray = deviceInfo.quirks;
      string quirksStr;
      for (uint32_t i = 0; i < quirksArray.Length(); i++) {
        if (i > 0) quirksStr += ",";
        quirksStr += quirksArray[i].Value();
      }
      platformConfig.deviceInfo.quirks = std::move(quirksStr);

      // Extract mimeTypeExclusions from JsonObject and assign to struct fields
      auto& mimeExclusions = deviceInfo.mimeTypeExclusions;
      if (mimeExclusions.HasLabel("CDVR")) {
        auto cdvrArray = mimeExclusions["CDVR"].Array();
        string cdvrStr;
        for (uint32_t i = 0; i < cdvrArray.Length(); i++) {
          if (i > 0) cdvrStr += ",";
          cdvrStr += cdvrArray[i].String();
        }
        platformConfig.deviceInfo.mimeTypeExclusions.cdvr = std::move(cdvrStr);
      }
      if (mimeExclusions.HasLabel("DVR")) {
        auto dvrArray = mimeExclusions["DVR"].Array();
        string dvrStr;
        for (uint32_t i = 0; i < dvrArray.Length(); i++) {
          if (i > 0) dvrStr += ",";
          dvrStr += dvrArray[i].String();
        }
        platformConfig.deviceInfo.mimeTypeExclusions.dvr = std::move(dvrStr);
      }
      if (mimeExclusions.HasLabel("EAS")) {
        auto easArray = mimeExclusions["EAS"].Array();
        string easStr;
        for (uint32_t i = 0; i < easArray.Length(); i++) {
          if (i > 0) easStr += ",";
          easStr += easArray[i].String();
        }
        platformConfig.deviceInfo.mimeTypeExclusions.eas = std::move(easStr);
      }
      if (mimeExclusions.HasLabel("IPDVR")) {
        auto ipdvrArray = mimeExclusions["IPDVR"].Array();
        string ipdvrStr;
        for (uint32_t i = 0; i < ipdvrArray.Length(); i++) {
          if (i > 0) ipdvrStr += ",";
          ipdvrStr += ipdvrArray[i].String();
        }
        platformConfig.deviceInfo.mimeTypeExclusions.ipdvr = std::move(ipdvrStr);
      }
      if (mimeExclusions.HasLabel("IVOD")) {
        auto ivodArray = mimeExclusions["IVOD"].Array();
        string ivodStr;
        for (uint32_t i = 0; i < ivodArray.Length(); i++) {
          if (i > 0) ivodStr += ",";
          ivodStr += ivodArray[i].String();
        }
        platformConfig.deviceInfo.mimeTypeExclusions.ivod = std::move(ivodStr);
      }
      if (mimeExclusions.HasLabel("LINEAR_TV")) {
        auto linearTVArray = mimeExclusions["LINEAR_TV"].Array();
        string linearTVStr;
        for (uint32_t i = 0; i < linearTVArray.Length(); i++) {
          if (i > 0) linearTVStr += ",";
          linearTVStr += linearTVArray[i].String();
        }
        platformConfig.deviceInfo.mimeTypeExclusions.linearTV = std::move(linearTVStr);
      }
      if (mimeExclusions.HasLabel("VOD")) {
        auto vodArray = mimeExclusions["VOD"].Array();
        string vodStr;
        for (uint32_t i = 0; i < vodArray.Length(); i++) {
          if (i > 0) vodStr += ",";
          vodStr += vodArray[i].String();
        }
        platformConfig.deviceInfo.mimeTypeExclusions.vod = std::move(vodStr);
      }

      // Extract features from JsonObject and assign to struct fields
      auto& featuresObj = deviceInfo.features;
      if (featuresObj.HasLabel("allowSelfSignedWithIPAddress")) {
        platformConfig.deviceInfo.features.allowSelfSignedWithIPAddress = featuresObj["allowSelfSignedWithIPAddress"].Number();
      }
      if (featuresObj.HasLabel("connection.supportsSecure")) {
        platformConfig.deviceInfo.features.supportsSecure = featuresObj["connection.supportsSecure"].Number();
      }
      if (featuresObj.HasLabel("htmlview.callJavaScriptWithResult")) {
        platformConfig.deviceInfo.features.callJavaScriptWithResult = featuresObj["htmlview.callJavaScriptWithResult"].Number();
      }
      if (featuresObj.HasLabel("htmlview.cookies")) {
        platformConfig.deviceInfo.features.cookies = featuresObj["htmlview.cookies"].Number();
      }
      if (featuresObj.HasLabel("htmlview.disableCSSAnimations")) {
        platformConfig.deviceInfo.features.disableCSSAnimations = featuresObj["htmlview.disableCSSAnimations"].Number();
      }
      if (featuresObj.HasLabel("htmlview.evaluateJavaScript")) {
        platformConfig.deviceInfo.features.evaluateJavaScript = featuresObj["htmlview.evaluateJavaScript"].Number();
      }
      if (featuresObj.HasLabel("htmlview.headers")) {
        platformConfig.deviceInfo.features.headers = featuresObj["htmlview.headers"].Number();
      }
      if (featuresObj.HasLabel("htmlview.httpCookies")) {
        platformConfig.deviceInfo.features.httpCookies = featuresObj["htmlview.httpCookies"].Number();
      }
      if (featuresObj.HasLabel("htmlview.postMessage")) {
        platformConfig.deviceInfo.features.postMessage = featuresObj["htmlview.postMessage"].Number();
      }
      if (featuresObj.HasLabel("htmlview.urlpatterns")) {
        platformConfig.deviceInfo.features.urlpatterns = featuresObj["htmlview.urlpatterns"].Number();
      }
      if (featuresObj.HasLabel("keySource")) {
        platformConfig.deviceInfo.features.keySource = featuresObj["keySource"].Number();
      }
      if (featuresObj.HasLabel("uhd_4k_decode")) {
        platformConfig.deviceInfo.features.uhd4kDecode = featuresObj["uhd_4k_decode"].Number();
      }

      // Convert mimeTypes array to comma-separated string
      auto& mimeTypesArray = deviceInfo.mimeTypes;
      string mimeTypesStr;
      for (uint32_t i = 0; i < mimeTypesArray.Length(); i++) {
        if (i > 0) mimeTypesStr += ",";
        mimeTypesStr += mimeTypesArray[i].Value();
      }
      platformConfig.deviceInfo.mimeTypes = std::move(mimeTypesStr);

      platformConfig.deviceInfo.model = deviceInfo.model.Value();
      platformConfig.deviceInfo.deviceType = deviceInfo.deviceType.Value();
      platformConfig.deviceInfo.supportsTrueSD = deviceInfo.supportsTrueSD.Value();
      platformConfig.deviceInfo.webBrowser.browserType = deviceInfo.webBrowser.browserType.Value();
      platformConfig.deviceInfo.webBrowser.version = deviceInfo.webBrowser.version.Value();
      platformConfig.deviceInfo.webBrowser.userAgent = deviceInfo.webBrowser.userAgent.Value();
      platformConfig.deviceInfo.hdrCapability = deviceInfo.HdrCapability.Value();
      platformConfig.deviceInfo.canMixPCMWithSurround = deviceInfo.canMixPCMWithSurround.Value();
      platformConfig.deviceInfo.publicIP = deviceInfo.publicIP.Value();
    }
  } else {
    TRACE(Trace::Error, (_T("%s Bad query '%s'\n"),
        __FILE__, query.c_str()));
    result = false;
  }

  success = result;
  Add(_T("success"), &success);
  platformConfig.success = result;

  return result;
}

bool PlatformCaps::AccountInfo::Load(PluginHost::IShell* service, const string &query) {
  bool result = true;

  Reset();

  PlatformCapsData data(service);

  if (query.empty() || query == _T("accountId")) {
    accountId = data.GetAccountId();
    Add(_T("accountId"), &accountId);
  }

  if (query.empty() || query == _T("x1DeviceId")) {
    x1DeviceId = data.GetX1DeviceId();
    Add(_T("x1DeviceId"), &x1DeviceId);
  }

  if (query.empty() || query == _T("XCALSessionTokenAvailable")) {
    XCALSessionTokenAvailable = data.XCALSessionTokenAvailable();
    Add(_T("XCALSessionTokenAvailable"), &XCALSessionTokenAvailable);
  }

  if (query.empty() || query == _T("experience")) {
    experience = data.GetExperience();
    Add(_T("experience"), &experience);
  }

  if (query.empty() || query == _T("deviceMACAddress")) {
    deviceMACAddress = data.GetDdeviceMACAddress();
    Add(_T("deviceMACAddress"), &deviceMACAddress);
  }

  if (query.empty() || query == _T("firmwareUpdateDisabled")) {
    firmwareUpdateDisabled = data.GetFirmwareUpdateDisabled();
    Add(_T("firmwareUpdateDisabled"), &firmwareUpdateDisabled);
  }

  return result;
}

bool PlatformCaps::DeviceInfo::Load(PluginHost::IShell* service, const string &query) {
  bool result = true;

  Reset();

  PlatformCapsData data(service);

  if (query.empty() || query == _T("quirks")) {
    quirks.Clear();
    auto q = data.GetQuirks();
    for (const auto &value: q)
      quirks.Add() = value;
    Add(_T("quirks"), &quirks);
  }

  if (query.empty() || query == _T("mimeTypeExclusions")) {
    mimeTypeExclusions.Clear();
    std::map <string, std::list<string>> hash;
    data.AddDashExclusionList(hash);
    if (!hash.empty()) {
      for (auto &it: hash) {
        JsonArray jsonArray;
        for (auto &jt: it.second) {
          jsonArray.Add() = jt;
        }
        mimeTypeExclusions[it.first.c_str()] = jsonArray;
      }
      Add(_T("mimeTypeExclusions"), &mimeTypeExclusions);
    }
  }

  if (query.empty() || query == _T("features")) {
    features.Clear();
    auto hash = data.DeviceCapsFeatures();
    if (!hash.empty()) {
      for (auto &it: hash) {
        features[it.first.c_str()] = it.second;
      }
      Add(_T("features"), &features);
    }
  }

  if (query.empty() || query == _T("mimeTypes")) {
    mimeTypes.Clear();
    auto q = data.GetMimeTypes();
    for (const auto &value: q)
      mimeTypes.Add() = value;
    Add(_T("mimeTypes"), &mimeTypes);
  }

  if (query.empty() || query == _T("model")) {
    model = data.GetModel();
    Add(_T("model"), &model);
  }

  if (query.empty() || query == _T("deviceType")) {
    deviceType = data.GetDeviceType();
    Add(_T("deviceType"), &deviceType);
  }

  if (query.empty() || query == _T("supportsTrueSD")) {
    supportsTrueSD = data.SupportsTrueSD();
    Add(_T("supportsTrueSD"), &supportsTrueSD);
  }

  if (query.empty() || query == _T("webBrowser")) {
    auto b = data.GetBrowser();
    webBrowser.browserType = std::get<0>(b);
    webBrowser.version = std::get<1>(b);
    webBrowser.userAgent = std::get<2>(b);
    Add(_T("webBrowser"), &webBrowser);
  }

  if (query.empty() || query == _T("HdrCapability")) {
    HdrCapability = data.GetHDRCapability();
    Add(_T("HdrCapability"), &HdrCapability);
  }

  if (query.empty() || query == _T("canMixPCMWithSurround")) {
    canMixPCMWithSurround = data.CanMixPCMWithSurround();
    Add(_T("canMixPCMWithSurround"), &canMixPCMWithSurround);
  }

  if (query.empty() || query == _T("publicIP")) {
    publicIP = data.GetPublicIP();
    Add(_T("publicIP"), &publicIP);
  }

  return result;
}

} // namespace Plugin
} // namespace WPEFramework
