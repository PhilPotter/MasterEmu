/* MasterEmu zip browser source code file
   copyright Phil Potter, 2024 */

package uk.co.philpotter.masteremu;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;

/**
 * This class loads a save state and then exits.
 */
public class ZipBrowser extends Activity {

    // define instance variables
    private long emulatorContainerPointer;
    private String checksumStr;
    private PathClickListener pc;
    private File deletionHolder;

    // native methods to call
    public native void loadStateStub(long emulatorContainerPointer, String fileName);

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
     * This method saves the extra bundle and allows us to handle screen
     * reorientations properly.
     */
    @Override
    public void onSaveInstanceState(Bundle savedInstanceState) {
        // transfer mappings
        Bundle extra = getIntent().getBundleExtra("MAIN_BUNDLE");
        savedInstanceState.putAll(extra);
    }

    /**
     * This method creates a list of all the save states and lets us select one.
     */
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.statebrowser_activity);

        deletionHolder = null;

        Bundle b = null;
        if (savedInstanceState == null)
            b = getIntent().getBundleExtra("MAIN_BUNDLE");
        else
            b = savedInstanceState;
        long lowerAddress = b.getInt("lowerAddress");
        long higherAddress = b.getInt("higherAddress");
        emulatorContainerPointer = ((0xFFFFFFFF00000000L & (higherAddress << 32)) | (0xFFFFFFFFL & lowerAddress));
        checksumStr = b.getString("CHECKSUM");

        ListView lv = (ListView)findViewById(R.id.statelist);
        pc = new PathClickListener();
        lv.setOnItemClickListener(pc);
        lv.setOnItemLongClickListener(pc);

        populateStateList(checksumStr);

        // Set focus
        lv.requestFocus();
    }

    /**
     * This method finds all files and folders in the supplied path
     * and populates the grid with them.
     */
    public void populateStateList(String path) {
        File filesDir = new File(this.getFilesDir() + "/" + path);
        File[] tempFileArray = filesDir.listFiles();
        File[] tempCanonicalFileArray = new File[tempFileArray.length];
        for (int i = 0; i < tempFileArray.length; ++i) {
            try {
                tempCanonicalFileArray[i] = new File(tempFileArray[i].getCanonicalPath());
            }
            catch (IOException e) {
                Log.e("populateStateList:", "Couldn't get canonical path");
                System.exit(0);
            }
        }
        ArrayList<File> tempList = new ArrayList<>();

        for (int i = 0; i < tempCanonicalFileArray.length; ++i) {
            if (tempCanonicalFileArray[i].getName().toLowerCase().endsWith(".mesav") &&
                    !tempCanonicalFileArray[i].getName().toLowerCase().equals("current_state.mesav")) {
                tempList.add(tempCanonicalFileArray[i]);
            }
        }

        Collections.sort(tempList);

        if (tempList.size() == 0) {
            TextView no_states_view = (TextView)findViewById(R.id.no_states_view);
            no_states_view.setText("No save states in this folder");
        } else {
            ListView lv = (ListView)findViewById(R.id.statelist);
            lv.setAdapter(new FilesystemAdapter(this, tempList.toArray()));
        }
    }

    /**
     * This class allows us to present a list of File objects as the
     * backing store of a ListView.
     */
    protected class FilesystemAdapter extends BaseAdapter {

        private Object[] filesAndFolders;
        private Context context;
        private LayoutInflater inflater;

        public FilesystemAdapter(Context c, Object[] filesAndFolders) {
            this.context = c;
            this.filesAndFolders = filesAndFolders;
            this.inflater = LayoutInflater.from(c);
        }

        public void deleteFromList(File f) {
            for (int i = 0; i < filesAndFolders.length; ++i) {
                if (f == filesAndFolders[i]) {
                    ArrayList<Object> temp = new ArrayList<Object>();
                    for (int j = 0; j < filesAndFolders.length; ++j) {
                        if (f != filesAndFolders[j]) {
                            temp.add(filesAndFolders[j]);
                        }
                    }
                    filesAndFolders = temp.toArray();
                    break;
                }
            }
            notifyDataSetChanged();
        }

        @Override
        public int getCount() {
            return filesAndFolders.length;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {

            View v = null;
            if (position < 0 || position > getCount() - 1)
                return v;

            if (convertView != null) {
                v = convertView;
            } else {
                v = inflater.inflate(R.layout.state_row, parent, false);
            }

            TextView path = (TextView)((ViewGroup)v).getChildAt(0);
            String name = ((File)filesAndFolders[position]).getName();
            path.setText(name);

            return v;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public Object getItem(int position) {
            return filesAndFolders[position];
        }

    }

    /**
     * This class allows us to listen for clicks on items in the
     * ListView.
     */
    private class PathClickListener implements AdapterView.OnItemClickListener,AdapterView.OnItemLongClickListener {
        @Override
        public void onItemClick(AdapterView av, View v, int position, long id) {
            File file = (File)av.getItemAtPosition(position);
            loadStateStub(ZipBrowser.this.emulatorContainerPointer, file.getName());
        }

        @Override
        public boolean onItemLongClick(AdapterView av, View v, int position, long id) {
            deletionHolder = (File)av.getItemAtPosition(position);

            // create dialogue
            AlertDialog deleteMenu = new AlertDialog.Builder(ZipBrowser.this).create();
            deleteMenu.setTitle("Deletion Prompt");
            deleteMenu.setMessage("Are you sure you want to delete " + deletionHolder.getName() + "?");
            DeletionListener dl = new DeletionListener();
            deleteMenu.setButton(DialogInterface.BUTTON_POSITIVE, "YES", dl);
            deleteMenu.setButton(DialogInterface.BUTTON_NEGATIVE, "NO", dl);
            deleteMenu.show();

            return true;
        }
    }

    /**
     * This is the listener which handles deletion of a state.
     */
    private class DeletionListener implements DialogInterface.OnClickListener {
        @Override
        public void onClick(DialogInterface dialog, int which) {
            switch (which) {
            case DialogInterface.BUTTON_POSITIVE:
                if (deletionHolder != null) {
                    deletionHolder.delete();
                    ListView lv = (ListView) findViewById(R.id.statelist);
                    FilesystemAdapter fsa = (FilesystemAdapter)lv.getAdapter();
                    fsa.deleteFromList(deletionHolder);
                    deletionHolder = null;
                    ZipBrowser.this.deleteStateSucceeded();
                } else {
                    ZipBrowser.this.deleteStateFailed();
                }
                break;
            case DialogInterface.BUTTON_NEGATIVE:
                ZipBrowser.this.deleteStateCancelled();
                break;
            }
        }
    }

    public void deleteStateFailed() {
        Toast failed = Toast.makeText(ZipBrowser.this, "State deletion failed", Toast.LENGTH_SHORT);
        failed.show();
    }

    public void deleteStateSucceeded() {
        Toast success = Toast.makeText(ZipBrowser.this, "State deleted", Toast.LENGTH_SHORT);
        success.show();
    }

    public void deleteStateCancelled() {
        Toast cancelled = Toast.makeText(ZipBrowser.this, "State not deleted", Toast.LENGTH_SHORT);
        cancelled.show();
    }

    public void loadStateSucceeded() {
        Toast success = Toast.makeText(ZipBrowser.this, "Successfully loaded state", Toast.LENGTH_SHORT);
        success.show();
        finish();
    }

    public void loadStateFailed() {
        Toast failed = Toast.makeText(ZipBrowser.this, "Failed to load state", Toast.LENGTH_SHORT);
        failed.show();
        finish();
    }
}
