/* MasterEmu titlescreen source code file
   copyright Phil Potter, 2019 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.os.Bundle;

/**
 * This class is for informing the user they installed the wrong APK outside the store
 */
public class WrongCpuActivity extends Activity {

    /**
     * This method creates the message to reinstall from Google Play
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.wrongcpu_activity);
    }
}
