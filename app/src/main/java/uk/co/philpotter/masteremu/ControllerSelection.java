/* MasterEmu controller selection source code file
   copyright Phil Potter, 2024 */

package uk.co.philpotter.masteremu;

import java.util.List;
import java.util.ArrayList;

/**
 * This class allows a list of ControllerTextView references to be manipulated.
 */
public class ControllerSelection {

    // instance variables
    private List<ControllerMapped> list;
    private ControllerMapped currentlySelected;
    private boolean isFileBrowser = false;

    public ControllerSelection() {
        list = new ArrayList<ControllerMapped>();
    }

    public void addMapping(ControllerMapped c) {
        list.add(c);
    }

    public void setSelected(ControllerMapped c) {
        for (ControllerMapped cm : list) {
            cm.unHighlight();
        }
        c.highlight();
        currentlySelected = c;
    }

    public ControllerMapped getSelected() {
        if (currentlySelected != null)
            return currentlySelected;
        else
            return null;
    }

    public void activate() {
        if (currentlySelected != null)
            currentlySelected.activate();
    }

    public void moveDown() {
        int size = list.size();
        if (currentlySelected != null) {
            if (size > 0) {
                int nextPos = (list.indexOf(currentlySelected) + 1) % list.size();
                setSelected(list.get(nextPos));
            }
        } else {
            if (size > 0) {
                if (!isFileBrowser)
                    setSelected(list.get(0));
                else
                    setSelected(list.get(1));
            }
        }
    }

    public void moveUp() {
        int size = list.size();
        if (currentlySelected != null) {
            if (size > 0) {
                int nextPos = list.indexOf(currentlySelected) - 1;
                if (nextPos < 0)
                    nextPos += list.size();
                setSelected(list.get(nextPos));
            }
        } else {
            if (size > 0) {
                if (!isFileBrowser)
                    setSelected(list.get(0));
                else if (size > 1)
                    setSelected(list.get(1));
            }
        }
    }

    public void isFileBrowser() {
        isFileBrowser = true;
    }

    public void reset() {
        currentlySelected = null;
    }
}
