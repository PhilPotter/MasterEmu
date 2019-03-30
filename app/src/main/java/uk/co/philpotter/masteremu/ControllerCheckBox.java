/* MasterEmu ControllerCheckBox source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.widget.CheckBox;

/**
 * Extends the normal CheckBox to allow better controller support.
 */
public class ControllerCheckBox extends CheckBox implements ControllerMapped {

    // instance variables
    Drawable inactiveDrawable;
    Drawable activeDrawable;

    /**
     * Constructors to allow instantiation in the same way as parent class.
     */
    public ControllerCheckBox(Context context) {
        super(context);
    }

    public ControllerCheckBox(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public ControllerCheckBox(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public void activate() {
        this.toggle();
    }

    public void highlight() {
        this.setSelected(true);
        setBackground(activeDrawable);
    }

    public void unHighlight() {
        this.setSelected(false);
        setBackground(null);
    }

    public void setActiveDrawable(Drawable d) {
        activeDrawable = d;
    }

    public void setInactiveDrawable(Drawable d) {
        inactiveDrawable = d;
    }
}
