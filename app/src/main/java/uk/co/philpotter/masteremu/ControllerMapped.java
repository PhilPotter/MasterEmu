/* MasterEmu ControllerMapped interface file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import android.graphics.drawable.Drawable;
import android.content.Intent;

/**
 * This interface allows us to treat all objects equally in terms of highlighting them etc.
 */
public interface ControllerMapped {
    public void highlight();
    public void unHighlight();
    public void activate();
    public void setActiveDrawable(Drawable d);
    public void setInactiveDrawable(Drawable d);
}
