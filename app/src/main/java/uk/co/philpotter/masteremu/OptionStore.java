/* MasterEmu option store source code file
   copyright Phil Potter, 2024 */

package uk.co.philpotter.masteremu;

import java.io.File;
import java.io.BufferedReader;
import java.io.FileReader;
import android.util.Log;
import java.io.IOException;
import java.io.FileNotFoundException;

/** This class stores options statically so they can be easily
 * referenced by any activity.
 */
public class OptionStore {

    static public boolean orientation_lock;
    static public boolean disable_sound;
    static public String orientation;
    static public boolean larger_buttons;
    static public boolean no_buttons;
    static public boolean japanese_mode;
    static public boolean no_stretching;
    static public String default_path;
    static public boolean game_genie;

    static public void updateOptionsFromFile(String filePath) {
        File settingsFile = new File(filePath);

        // Open settings file
        BufferedReader settingsReader = null;
        try {
            settingsReader = new BufferedReader(new FileReader(settingsFile));
            String temp;
            OptionStore.default_path = "";
            while ((temp = settingsReader.readLine()) != null) {
                String setting[] = temp.split("=", 0);
                if (setting[0].equals("orientation_lock")) {
                    if (setting[1].equals("1")) {
                        OptionStore.orientation_lock = true;
                    } else {
                        OptionStore.orientation_lock = false;
                    }
                } else if (setting[0].equals("disable_sound")) {
                    if (setting[1].equals("1")) {
                        OptionStore.disable_sound = true;
                    } else {
                        OptionStore.disable_sound = false;
                    }
                } else if (setting[0].equals("orientation")) {
                    if (setting[1].equals("portrait")) {
                        OptionStore.orientation = "portrait";
                    } else {
                        OptionStore.orientation = "landscape";
                    }
                } else if (setting[0].equals("larger_buttons")) {
                    if (setting[1].equals("1")) {
                        OptionStore.larger_buttons = true;
                    } else {
                        OptionStore.larger_buttons = false;
                    }
                } else if (setting[0].equals("no_buttons")) {
                    if (setting[1].equals("1")) {
                        OptionStore.no_buttons = true;
                    } else {
                        OptionStore.no_buttons = false;
                    }
                } else if (setting[0].equals("japanese_mode")) {
                    if (setting[1].equals("1")) {
                        OptionStore.japanese_mode = true;
                    } else {
                        OptionStore.japanese_mode = false;
                    }
                } else if (setting[0].equals("no_stretching")) {
                    if (setting[1].equals("1")) {
                        OptionStore.no_stretching = true;
                    } else {
                        OptionStore.no_stretching = false;
                    }
                } else if (setting[0].equals("default_path")) {
                    if (setting.length == 2 && setting[1].length() > 0) {
                        OptionStore.default_path = setting[1];
                    }
                } else if (setting[0].equals("game_genie")) {
                    if (setting[1].equals("1")) {
                        OptionStore.game_genie = true;
                    } else {
                        OptionStore.game_genie = false;
                    }
                }
            }
        }
        catch (FileNotFoundException e) {
            Log.v("OptionsStore", "Options file doesn't exist yet: " + e);
            OptionStore.orientation_lock = false;
            OptionStore.disable_sound = false;
            OptionStore.orientation = "";
            OptionStore.larger_buttons = false;
            OptionStore.no_buttons = false;
            OptionStore.japanese_mode = false;
            OptionStore.no_stretching = false;
            OptionStore.default_path = "";
            OptionStore.game_genie = false;
        }
        catch (IOException e) {
            Log.e("OptionStore", "Problem reading settings file: " + e);
        }
        finally {
            // Close settings file
            try {
                if (settingsReader != null)
                    settingsReader.close();
            }
            catch (IOException e) {
                Log.e("OptionStore", "Couldn't close settings file: " + e);
            }
        }
    }
}
