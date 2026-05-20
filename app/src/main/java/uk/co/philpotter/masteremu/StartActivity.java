/* MasterEmu start (and former ad consent) screen source code file
   copyright Phil Potter, 2024 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.os.Bundle;
import android.content.Intent;
import android.view.View;

/**
 * This class acts as the start screen of the app, just passing through.
 */
public class StartActivity extends Activity {

    /**
     * This method just starts the rest of the app now.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Pass straight through
        Intent titlescreenActivity = new Intent(this, TitlescreenActivity.class);
        titlescreenActivity.putExtra("startingApp", true);
        startActivity(titlescreenActivity);
        finish();
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