/* MasterEmu ZIP file ROM picker source code file
   copyright Phil Potter, 2022 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * This class loads a save state and then exits.
 */
public class ZipRomPicker extends Activity {

    // Store ZipInputStream here
    protected static ZipInputStream zis;
    protected static String ext;

    // define instance variables
    private PathClickListener pc;
    private Uri zipUri;

    /**
     * This method sets the screen orientation when locked.
     */
    @Override
    protected void onStart() {
        super.onStart();
        if (OptionStore.orientation_lock) {
            if (OptionStore.orientation.equals("portrait")) {
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            } else if (OptionStore.orientation.equals("landscape")) {
               setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            }
        } else {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
        }
    }

    /**
     * This method creates a list of all the save states and lets us select one.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.ziprompicker_activity);

        // Retrieve URI
        if (android.os.Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            zipUri = getIntent().getParcelableExtra(FileBrowser.ZIPROMPICKER_URI_ID);
        } else {
            zipUri = getIntent().getParcelableExtra(
                    FileBrowser.ZIPROMPICKER_URI_ID,
                    android.net.Uri.class
            );
        }

        ListView lv = (ListView)findViewById(R.id.romlist);
        pc = new PathClickListener();
        lv.setOnItemClickListener(pc);

        populateRomList();

        // Set focus
        lv.requestFocus();
    }

    /**
     * This method finds all ROM files in the supplied ZIP URI and populates the list
     * with them.
     */
    public void populateRomList() {
        ZipInputStream zis = null;
        try {
            zis = new ZipInputStream(getContentResolver().openInputStream(zipUri));
        }
        catch (IOException e) {
            showMessage("Failed to open ZIP file");
            cancelFinish();
            return;
        }

        // Add entries to list
        ArrayList<String> zipEntryList = new ArrayList<>();
        boolean readAllEntries = false;
        while (!readAllEntries) {
            ZipEntry ze = null;
            try {
                ze = zis.getNextEntry();
            }
            catch (Exception e) {
                showMessage("Failed to get next ZIP entry");
                cancelFinish();
                return;
            }

            // Read entry and add to array list, or mark that we
            // have read all entries
            if (ze == null) {
                readAllEntries = true;
            } else {
                if (!ze.isDirectory()) {
                    String zeName = ze.getName();
                    String zeLowerCaseName = zeName.toLowerCase();
                    if (zeLowerCaseName.endsWith(".gg") || zeLowerCaseName.endsWith(".sms")) {
                        zipEntryList.add(zeName);
                    }
                }

                // Now close entry
                try {
                    zis.closeEntry();
                }
                catch (Exception e) {
                    showMessage("Failed to close ZIP entry");
                    cancelFinish();
                    return;
                }
            }
        }

        // Close zip input stream and sort list
        try {
            zis.close();
        }
        catch (IOException e) {
            showMessage("Failed to close ZIP file after parsing files within");
            cancelFinish();
            return;
        }
        Collections.sort(zipEntryList);

        if (zipEntryList.size() == 0) {
            TextView no_states_view = (TextView)findViewById(R.id.no_roms_view);
            no_states_view.setText("No ROM files in this ZIP file");
        } else {
            ListView lv = (ListView)findViewById(R.id.romlist);
            lv.setAdapter(new StringAdapter(this, zipEntryList));
        }
    }

    /**
     * This class allows us to present a list of strings as the backing store of a ListView.
     */
    protected class StringAdapter extends BaseAdapter {

        private List<String> zipEntryList;
        private Context context;
        private LayoutInflater inflater;

        public StringAdapter(Context c, List<String> zipEntryList) {
            this.context = c;
            this.zipEntryList = zipEntryList;
            this.inflater = LayoutInflater.from(c);
        }

        @Override
        public int getCount() {
            return zipEntryList.size();
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {

            View v = null;
            if (position < 0 || position > getCount() - 1)
                return v;

            if (convertView != null) {
                v = convertView;
            } else {
                v = inflater.inflate(R.layout.zipentry_row, parent, false);
            }

            TextView path = (TextView)((ViewGroup)v).getChildAt(0);
            String name = zipEntryList.get(position);
            path.setText(name);

            return v;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public Object getItem(int position) {
            return zipEntryList.get(position);
        }

    }

    /**
     * This class allows us to listen for clicks on items in the
     * ListView.
     */
    private class PathClickListener implements AdapterView.OnItemClickListener {
        @Override
        public void onItemClick(AdapterView av, View v, int position, long id) {
            // Get name we tapped on
            String zipEntryName = (String)av.getItemAtPosition(position);

            // Open stream again
            ZipInputStream zis = null;
            try {
                zis = new ZipInputStream(getContentResolver().openInputStream(zipUri));
            }
            catch (IOException e) {
                showMessage("Failed to open ZIP file");
                cancelFinish();
                return;
            }

            // Iterate until we find the correct entry
            boolean foundRom = false;
            while (!foundRom) {
                ZipEntry ze = null;
                try {
                    ze = zis.getNextEntry();
                } catch (Exception e) {
                    showMessage("Failed to get next ZIP entry");
                    cancelFinish();
                    return;
                }

                if (ze == null) {
                    break;
                } else {
                    if (ze.getName().equals(zipEntryName)) {
                        foundRom = true;
                    } else {
                        try {
                            zis.closeEntry();
                        } catch (Exception e) {
                            showMessage("Failed to close ZIP entry");
                            cancelFinish();
                            return;
                        }
                    }
                }
            }

            if (foundRom) {
                ZipRomPicker.zis = zis;
                ZipRomPicker.ext = zipEntryName.substring(zipEntryName.lastIndexOf('.')).toLowerCase();
                okFinish();
            } else {
                showMessage("ROM not found in ZIP file");
                cancelFinish();
            }
        }
    }

    /**
     * This shows a message.
     * @param message
     */
    private void showMessage(String message) {
        Toast messageToast = Toast.makeText(this, message, Toast.LENGTH_SHORT);
        messageToast.show();
    }

    /**
     * This just exits with a cancel code.
     */
    private void cancelFinish() {
        setResult(RESULT_CANCELED);
        zis = null;
        finish();
    }

    /**
     * This just exits with an OK code.
     */
    private void okFinish() {
        setResult(RESULT_OK);
        finish();
    }
}
