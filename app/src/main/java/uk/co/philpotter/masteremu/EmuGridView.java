/* MasterEmu EmuGridView source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.widget.GridView;
import android.content.Context;
import android.util.AttributeSet;

/**
 * This class allows automatic spacing between rows to prevent overlapping rows.
 */
public class EmuGridView extends GridView {

    // superclass constructors called here
    public EmuGridView(Context context) {
        super(context);
    }

    public EmuGridView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public EmuGridView(Context context, AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }
}
