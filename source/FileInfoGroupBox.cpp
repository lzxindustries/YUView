/*  YUView - YUV player with advanced analytics toolset
*   Copyright (C) 2015  Institut für Nachrichtentechnik
*                       RWTH Aachen University, GERMANY
*
*   YUView is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   YUView is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with YUView.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FileInfoGroupBox.h"
#include <assert.h>

/* The file info group box can display information on a file (or any other displayobject).
 * If you provide a list of QString tuples, this class will fill a grid layout with the 
 * corresponding labels.
 */
FileInfoGroupBox::FileInfoGroupBox(QWidget *parent) : QGroupBox(parent)
{
  // Set title
  setTitle("");

  p_gridLayout = new QGridLayout;
  setLayout(p_gridLayout);

  p_nrLabelPairs = 0;
}

FileInfoGroupBox::~FileInfoGroupBox()
{
  delete p_gridLayout;
}

void FileInfoGroupBox::setFileInfo()
{
  // Clear title
  setTitle("File Info");

  // Clear the grid layout
  foreach(QLabel *l, p_labelList) {
    p_gridLayout->removeWidget(l);
    delete l;
  }
  p_labelList.clear();
}

void FileInfoGroupBox::setFileInfo(QString fileInfoTitle, QList<infoItem> fileInfoList)
{
  // Set the title
  setTitle(fileInfoTitle);

  if (fileInfoList.count() == p_nrLabelPairs) {
    // The correct number of label pairs is already in the groupBox.
    // No need to delete all items and reattach them. Just update the text.
    for (int i = 0; i < p_nrLabelPairs; i++)
    {
      assert(p_nrLabelPairs * 2 == p_labelList.count());

      p_labelList[i * 2    ]->setText(fileInfoList[i].first);
      p_labelList[i * 2 + 1]->setText(fileInfoList[i].second);
    }
  }
  else {
    // Update the grid layout. Delete all the labels and add as many new ones as necessary.

    // Clear the grid layout
    foreach(QLabel *l, p_labelList) {
      p_gridLayout->removeWidget(l);
      delete l;
    }
    p_labelList.clear();

    // For each item in the list add a two labels to the grid layout
    int i = 0;
    foreach(infoItem info, fileInfoList) {
      // Create labels
      QLabel *newTextLabel = new QLabel(info.first);
      QLabel *newValueLabel = new QLabel(info.second);
      newValueLabel->setWordWrap(true);

      // Add to grid
      p_gridLayout->addWidget(newTextLabel, i, 0);
      p_gridLayout->addWidget(newValueLabel, i, 1);

      // Set row stretch to 0
      p_gridLayout->setRowStretch(i, 0);

      i++;

      // Add to list of labels
      p_labelList.append(newTextLabel);
      p_labelList.append(newValueLabel);
    }

    p_gridLayout->setColumnStretch(1, 1);	///< Set the second column to strectch
    p_gridLayout->setRowStretch(i, 1);		///< Set the last rwo to strectch

    p_nrLabelPairs = i;
  }
}

void FileInfoGroupBox::currentSelectedItemsChanged(playlistItem *item1, playlistItem *item2)
{
  // Only show the info of the first selection
  // TODO: why not show both?
  if (item1)
    setFileInfo( item1->getInfoTitel(), item1->getInfoList() );
  else
    setFileInfo();
}