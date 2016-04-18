#include "LauncherComponent.h"
#include "AppsPageComponent.h"
#include "LibraryPageComponent.h"
#include "SettingsPageComponent.h"
#include "PowerPageComponent.h"

#include "Main.h"
#include "Utils.h"
#include <math.h>

void LaunchSpinnerTimer::timerCallback() {
  if (launcherComponent) {
    auto lsp = launcherComponent->launchSpinner.get();
    const auto& lspImg = launcherComponent->launchSpinnerImages;
    
    i++;
    if (i == lspImg.size()) { i = 0; }
    lsp->setImage(lspImg[i]);
  }
}

void BatteryIconTimer::timerCallback() {
  
  // get current battery status from the battery monitor thread
  auto batteryStatus = launcherComponent->batteryMonitor.getCurrentStatus();
  
  // we can't change anything if we don't have a LauncherComponent
  if(launcherComponent) {
    
    // we want to modify the "Battery" icon
      const auto& batteryIcons = launcherComponent->batteryIconImages;
      const auto& batteryIconsCharging = launcherComponent->batteryIconChargingImages;
      

    for( auto button : launcherComponent->topButtons->buttons ) {
      Image batteryImg = batteryIcons[3];
      if (button->getName() == "Battery") {
          int status = round( ((float)batteryStatus.percentage)/100.0f * 3.0f );
          if( batteryStatus.percentage <= 5 ) {
              status = 3;
          } else {
              // limit status range to [0:3]
              if(status < 0) status = 0;
              if(status > 2) status = 2;
          }
          if( !batteryStatus.isCharging ) {
              batteryImg = batteryIcons[status];
          } else {
              batteryImg = batteryIconsCharging[status];

          }
          
          button->setImages(true, true, true,                       //
                       batteryImg, 1.0f, Colours::transparentWhite, // normal
                       batteryImg, 1.0f, Colours::transparentWhite, // over
                       batteryImg, 1.0f, Colours::transparentWhite, // down
                       0);
      }
    }
  }
  
  //DBG( "Charging: "  << batteryStatus.isCharging );
  //DBG( "Voltage: " << batteryStatus.percentage );
  
}

void WifiIconTimer::timerCallback() {
  if(!launcherComponent) { return; }
    
  for( auto button : launcherComponent->topButtons->buttons ) {
    if (button->getName() == "WiFi") {
      Image wifiIcon;
      if (const auto& conAp = getWifiStatus().connectedAccessPoint()) {
        // -120f to 0
        float sigStrength = conAp->signalStrength;
        // 0.0 - 1.0
        int status = round( (sigStrength+120.0f)/120.0f );
        int idx = status * 3.0;
        wifiIcon = launcherComponent->wifiIconImages[idx];
      }
      else {
        wifiIcon = launcherComponent->wifiIconImages[0];
      }
      button->setImages(true, true, true,                       //
                        wifiIcon, 1.0f, Colours::transparentWhite, // normal
                        wifiIcon, 1.0f, Colours::transparentWhite, // over
                        wifiIcon, 0.3f, Colours::transparentWhite, // down
                        0);

    }
  }
}

LauncherComponent::LauncherComponent(const var &configJson)
{
  bgColor = Colour(0xff2e8dbd);
  bgImage = "mainBackground.png";
  pageStack = new PageStackComponent();
  addAndMakeVisible(pageStack);

  topButtons = new LauncherBarComponent();
  botButtons = new LauncherBarComponent();
  topButtons->setInterceptsMouseClicks(false, true);
  botButtons->setInterceptsMouseClicks(false, true);
  addAndMakeVisible(topButtons);
  addAndMakeVisible(botButtons);
  
  batteryMonitor.startThread();
  
  batteryIconTimer.launcherComponent = this;
  batteryIconTimer.startTimer(1000);
    
  wifiIconTimer.launcherComponent = this;
  wifiIconTimer.startTimer(5000);
    
  Array<String> wifiImgPaths{"wifiStrength0.png","wifiStrength1.png","wifiStrength2.png","wifiStrength3.png"};
  for(auto& path : wifiImgPaths) {
    auto image = createImageFromFile(assetFile(path));
    wifiIconImages.add(image);
  }
  
  Array<String> batteryImgPaths{"battery_1.png","battery_2.png","battery_3.png","battery_0.png"};
  for(auto& path : batteryImgPaths) {
    auto image = createImageFromFile(assetFile(path));
    batteryIconImages.add(image);
  }
    
  Array<String> batteryImgChargingPaths{"batteryCharging_1.png","batteryCharging_2.png","batteryCharging_3.png","batteryCharging_0.png"};
  for(auto& path : batteryImgChargingPaths) {
    auto image = createImageFromFile(assetFile(path));
    batteryIconChargingImages.add(image);
  }

  launchSpinnerTimer.launcherComponent = this;
  Array<String> spinnerImgPaths{"wait1.png","wait2.png","wait3.png","wait4.png"};
  for(auto& path : spinnerImgPaths) {
    auto image = createImageFromFile(assetFile(path));
    launchSpinnerImages.add(image);
  }
  launchSpinner = new ImageComponent();
  launchSpinner->setImage(launchSpinnerImages[0]);
  launchSpinner->setInterceptsMouseClicks(true, false);
  addChildComponent(launchSpinner);
  
  // Settings page
  auto settingsPage = new SettingsPageComponent();
  settingsPage->setName("Settings");
  pages.add(settingsPage);
  pagesByName.set("Settings", settingsPage);
  pagesByName.set("WiFi", settingsPage);
  
  // Power page
  auto powerPage = new PowerPageComponent();
  powerPage->setName("Power");
  pages.add(powerPage);
  pagesByName.set("Power", powerPage);
  pagesByName.set("Battery", powerPage);
  
  // Apps page
  auto appsPage = new AppsPageComponent(this);
  appsPage->setName("Apps");
  pages.add(appsPage);
  pagesByName.set("Apps", appsPage);
  
  // Apps library
  auto appsLibrary = new LibraryPageComponent();
  appsLibrary->setName("AppsLibrary");
  pages.add(appsLibrary);
  pagesByName.set("AppsLibrary", appsLibrary);
  
  // Read config for apps and corner locations
  auto pagesData = configJson["pages"].getArray();
  if (pagesData) {
    for (const auto &page : *pagesData) {
      auto name = page["name"].toString();
      if (name == "Apps") {
        
        appsPage->createIconsFromJsonArray(page["items"]);
        appsLibrary->createIconsFromJsonArray(page["items"]);
        auto buttonsData = *(page["cornerButtons"].getArray());
        
        // FIXME: is there a better way to slice juce Array<var> ?
        Array<var> topData{};
        Array<var> botData{};
        topData.add(buttonsData[0]);
        topData.add(buttonsData[1]);
        botData.add(buttonsData[2]);
        botData.add(buttonsData[3]);
        
        topButtons->addButtonsFromJsonArray(topData);
        botButtons->addButtonsFromJsonArray(botData);
        
        // NOTE(ryan): Maybe do something with a custom event later.. For now we just listen to all the
        // buttons manually.
        for (auto button : topButtons->buttons) {
          button->addListener(this);
          button->setTriggeredOnMouseDown(true);
        }
        for (auto button : botButtons->buttons) {
          button->addListener(this);
          button->setTriggeredOnMouseDown(true);
        }
        
      }
    }
  }

  defaultPage = pagesByName[configJson["defaultPage"]];
}

LauncherComponent::~LauncherComponent() {
  batteryIconTimer.stopTimer();
  batteryMonitor.stopThread(100);
}

void LauncherComponent::paint(Graphics &g) {
  g.fillAll(bgColor);
  auto image = createImageFromFile(assetFile(bgImage));
  g.drawImageAt(image,0,0,false);
}

void LauncherComponent::resized() {
  auto bounds = getLocalBounds();
  int barSize = 50;
  
  topButtons->setBounds(bounds.getX(), bounds.getY(), bounds.getWidth(),
                        barSize);
  botButtons->setBounds(bounds.getX(), bounds.getHeight() - barSize, bounds.getWidth(),
                             barSize);
  pageStack->setBounds(bounds.getX() + barSize, bounds.getY(), bounds.getWidth() - 2*barSize,
                       bounds.getHeight());
  launchSpinner->setBounds(bounds.getWidth()/3., 0, bounds.getWidth()/3., bounds.getHeight());

  // init
  if (!resize) {
    resize = true;
    pageStack->swapPage(defaultPage, PageStackComponent::kTransitionNone);
  }
}

void LauncherComponent::showLaunchSpinner() {
  DBG("Show launch spinner");
  launchSpinner->setVisible(true);
  launchSpinnerTimer.startTimer(1*1000);
}

void LauncherComponent::hideLaunchSpinner() {
  DBG("Hide launch spinner");
  launchSpinnerTimer.stopTimer();
  launchSpinner->setVisible(false);
}

void LauncherComponent::showAppsLibrary() {
  getMainStack().pushPage(pagesByName["AppsLibrary"], PageStackComponent::kTransitionTranslateHorizontalLeft);
}

void LauncherComponent::buttonClicked(Button *button) {
  auto currentPage = pageStack->getCurrentPage();
  if ((!currentPage || currentPage->getName() != button->getName()) &&
      pagesByName.contains(button->getName())) {
    auto page = pagesByName[button->getName()];
    if (button->getName() == "Settings" || button->getName() == "WiFi" ) {
      getMainStack().pushPage(page, PageStackComponent::kTransitionTranslateHorizontal);
    } else if (button->getName() == "Power" || button->getName() == "Battery" ) {
        getMainStack().pushPage(page, PageStackComponent::kTransitionTranslateHorizontalLeft);
    } else {
      pageStack->swapPage(page, PageStackComponent::kTransitionTranslateHorizontal);
    }
  }
}
