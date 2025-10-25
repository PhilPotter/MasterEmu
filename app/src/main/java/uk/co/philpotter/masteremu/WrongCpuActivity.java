/* MasterEmu titlescreen source code file
   copyright Phil Potter, 2024 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;

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

    @Override
    protected void onResume() {
        super.onResume();
        View decorView = getWindow().getDecorView();
        decorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
    }
}
