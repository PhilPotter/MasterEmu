/* MasterEmu TV start (and former ad consent) screen source code file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
/**
 * This class acts as the start screen of the app, just passing through.
 */
public class TvStartActivity extends Activity {


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
}