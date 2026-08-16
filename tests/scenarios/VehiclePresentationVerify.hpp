#ifndef LOKA_TESTS_SCENARIOS_VEHICLE_PRESENTATION_VERIFY_HPP
#define LOKA_TESTS_SCENARIOS_VEHICLE_PRESENTATION_VERIFY_HPP

#include <cstddef>
#include <vector>

#include "../support/MenuPresentationVerify.hpp"
#include "../support/TestVerify.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/Window.hpp"

namespace loka
{
  namespace scenario_tests
  {
    /** Verifies that a scenario presentation forwards the example's
        production window and menu declarations without maintaining a twin. */
    template <class ProductionConfig, class VehiclePresentation>
    void VerifyVehiclePresentation(PlatformContext *context,
                                   ProductionConfig &production,
                                   VehiclePresentation &vehicle,
                                   bool expectProductionMenu)
    {
      AppComposition productionComposition(context);
      AppComposition vehicleComposition(context);
      production.compose(productionComposition);
      vehicle.compose(vehicleComposition);
      std::vector<AppComponent *> productionComponents = productionComposition.build();
      std::vector<AppComponent *> vehicleComponents = vehicleComposition.build();

      LOKA_VERIFY(productionComponents.size() == 1);
      LOKA_VERIFY(vehicleComponents.size() == 1);
      Window *productionWindow = productionComponents[0] ? productionComponents[0]->asWindow() : 0;
      Window *vehicleWindow = vehicleComponents[0] ? vehicleComponents[0]->asWindow() : 0;
      LOKA_VERIFY(productionWindow != 0);
      LOKA_VERIFY(vehicleWindow != 0);
      LOKA_VERIFY(vehicleWindow->titleState().get().equals(productionWindow->titleState().get()));
      LOKA_VERIFY(vehicleWindow->frameState().get() == productionWindow->frameState().get());
      LOKA_VERIFY(vehicleWindow->visibilityState().get() == productionWindow->visibilityState().get());

      app::MenuBarDefinition productionMenu;
      app::MenuBarDefinition vehicleMenu;
      loka::testing::ComposeMenuBar(production, productionMenu);
      loka::testing::ComposeMenuBar(vehicle, vehicleMenu);
      LOKA_VERIFY(!expectProductionMenu || !productionMenu.empty());
      LOKA_VERIFY(loka::testing::MenuPresentationsEqual(productionMenu, vehicleMenu));

      for (std::size_t i = 0; i < productionComponents.size(); ++i)
      {
        delete productionComponents[i];
      }
      for (std::size_t i = 0; i < vehicleComponents.size(); ++i)
      {
        delete vehicleComponents[i];
      }
    }
  } // namespace scenario_tests
} // namespace loka

#endif // LOKA_TESTS_SCENARIOS_VEHICLE_PRESENTATION_VERIFY_HPP
