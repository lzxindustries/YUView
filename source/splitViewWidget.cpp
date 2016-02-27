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

#include "splitViewWidget.h"

#include <QPainter>
#include <QColor>
#include <QMouseEvent>
#include <QSettings>
#include <QDebug>
#include <QTextDocument>

#include "typedef.h"
#include "playlistTreeWidget.h"
#include "playbackController.h"

splitViewWidget::splitViewWidget(QWidget *parent)
  : QWidget(parent) , controls(new Ui::splitViewControlsWidget)
{
  setFocusPolicy(Qt::NoFocus);

  splittingPoint = 0.5;
  splittingDragging = false;
  setSplitEnabled(false);
  viewDragging = false;
  viewMode = SIDE_BY_SIDE;
  drawZoomBox = false;
  drawRegularGrid = false;
  regularGridSize = 64;
  zoomBoxMousePosition = QPoint();

  playlist = NULL;
  playback = NULL;

  updateSettings();

  centerOffset = QPoint(0, 0);
  zoomFactor = 1.0;
    
  // Initialize the font and the position of the zoom factor indication
  zoomFactorFont = QFont(SPLITVIEWWIDGET_ZOOMFACTOR_FONT, SPLITVIEWWIDGET_ZOOMFACTOR_FONTSIZE);
  QFontMetrics fm(zoomFactorFont);
  zoomFactorFontPos = QPoint( 10, fm.height() );

  // We want to have all mouse events (even move)
  setMouseTracking(true);
}

void splitViewWidget::setSplitEnabled(bool flag)
{
  if (splitting != flag)
  {
    // Value changed
    splitting = flag;

    // Update (redraw) widget
    update();
  }
}

/** The common settings might have changed.
  * Reload all settings from the Qsettings and set them.
  */
void splitViewWidget::updateSettings()
{
  // load the background color from settings and set it
  QPalette Pal(palette());
  QSettings settings;
  QColor bgColor = settings.value("Background/Color").value<QColor>();
  Pal.setColor(QPalette::Background, bgColor);
  setAutoFillBackground(true);
  setPalette(Pal);

  // Load the split line style from the settings and set it
  QString splittingStyleString = settings.value("SplitViewLineStyle").value<QString>();
  if (splittingStyleString == "Handlers")
    splittingLineStyle = TOP_BOTTOM_HANDLERS;
  else
    splittingLineStyle = SOLID_LINE;

  // Load zoom box background color
  zoomBoxBackgroundColor = settings.value("Background/Color").value<QColor>();
}

void splitViewWidget::paintEvent(QPaintEvent *paint_event)
{
  //qDebug() << paint_event->rect();

  if (!playlist)
    // The playlist was not initialized yet. Nothing to draw (yet)
    return;

  QPainter painter(this);
      
  // Get the full size of the area that we can draw on (from the paint device base)
  QPoint drawArea_botR(width(), height());

  // Get the current frame to draw
  int frame = playback->getCurrentFrame();

  // Get the playlist item(s) to draw
  playlistItem *item[2];
  playlist->getSelectedItems(item[0], item[1]);
  bool anyItemsSelected = item[0] != NULL || item[1] != NULL;

  // The x position of the split (if splitting)
  int xSplit = int(drawArea_botR.x() * splittingPoint);

  // First determine the center points per of each view
  QPoint centerPoints[2];
  if (viewMode == COMPARISON || !splitting)
  {
    // For comparison mode, both items have the same center point, in the middle of the view widget
    // This is equal to the scenario of not splitting
    centerPoints[0] = drawArea_botR / 2;
    centerPoints[1] = centerPoints[0];
  }
  else
  {
    // For side by side mode, the center points are centered in each individual split view 
    int y = drawArea_botR.y() / 2;
    centerPoints[0] = QPoint( xSplit / 2, y );
    centerPoints[1] = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, y );
  }

  // For the zoom box, calculate the pixel position under the cursor for each view. The following
  // things are calculated in this function:
  QPoint  pixelPos[2];                         //< The pixel position in the item under the cursor
  bool    pixelPosInItem[2] = {false, false};  //< Is the pixel position under the curser within the item?
  QRect   zoomPixelRect[2];                    //< A rect around the pixel that is under the cursor
  QPointF itemZoomBoxTranslation[2];           //< How do we have to translate the painter to draw the items in the zoom Box? (This still has to be scaled by the zoom factor that you want in the zoom box)
  if (anyItemsSelected && drawZoomBox && geometry().contains(zoomBoxMousePosition))
  {
    // Is the mouse over the left or the right item? (mouseInLeftOrRightView: false=left, true=right)
    int xSplit = int(drawArea_botR.x() * splittingPoint);
    bool mouseInLeftOrRightView = (splitting && (zoomBoxMousePosition.x() > xSplit));
    
    // The absolute center point of the item under the cursor
    QPoint itemCenterMousePos = (mouseInLeftOrRightView) ? centerPoints[1] + centerOffset : centerPoints[0] + centerOffset;
    
    // The difference in the item under the mouse (normalized by zoom factor)
    double diffInItem[2] = {(double)(itemCenterMousePos.x() - zoomBoxMousePosition.x()) / zoomFactor + 0.5, 
                            (double)(itemCenterMousePos.y() - zoomBoxMousePosition.y()) / zoomFactor + 0.5};

    // We now have the pixel difference value for the item under the cursor. 
    // We now draw one zoom box per view
    int viewNum = (splitting && item[1]) ? 2 : 1;
    for (int view=0; view<viewNum; view++)
    {
      // Get the size of the item
      double itemSize[2];
      itemSize[0] = item[view]->getVideoSize().width();
      itemSize[1] = item[view]->getVideoSize().height();
      
      // Calculate the position under the mouse cursor in pixels in the item under the mouse.
      {
        // Divide and round. We want valuew from 0...-1 to be quantized to -1 and not 0
        // so subtract 1 from the value if it is < 0.
        double pixelPosX = -diffInItem[0] + (itemSize[0] / 2) + 0.5;
        double pixelPoxY = -diffInItem[1] + (itemSize[1] / 2) + 0.5;
        if (pixelPosX < 0)
          pixelPosX -= 1;
        if (pixelPoxY < 0)
          pixelPoxY -= 1;
      
        pixelPos[view] = QPoint(pixelPosX, pixelPoxY);
      }

      // How do we have to translate the painter so that the current pixel is centered?
      // This is nedded when drawing the zoom boxes.
      itemZoomBoxTranslation[view] = QPointF( itemSize[0] / 2 - pixelPos[view].x() - 0.5, 
                                              itemSize[1] / 2 - pixelPos[view].y() - 0.5 );

      // Is the pixel under the cursor within the item?
      pixelPosInItem[view] = (pixelPos[view].x() >= 0 && pixelPos[view].x() < itemSize[0]) &&
                             (pixelPos[view].y() >= 0 && pixelPos[view].y() < itemSize[1]);

      // Mark the pixel under the cursor with a rect around it.
      if (pixelPosInItem[view])
      {
        int pixelPoint[2];
        pixelPoint[0] = -((itemSize[0] / 2 - pixelPos[view].x()) * zoomFactor);
        pixelPoint[1] = -((itemSize[1] / 2 - pixelPos[view].y()) * zoomFactor);
        zoomPixelRect[view] = QRect(pixelPoint[0], pixelPoint[1], zoomFactor, zoomFactor);
      }
    }
  }

  if (splitting)
  {
    // Draw two items (or less, if less items are selected)
    if (item[0])
    {
      // Set clipping to the left region
      QRegion clip = QRegion(0, 0, xSplit, drawArea_botR.y());
      painter.setClipRegion( clip );

      // Translate the painter to the position where we want the item to be
      painter.translate( centerPoints[0] + centerOffset );

      // Draw the item at position (0,0)
      item[0]->drawFrame( &painter, frame, zoomFactor );

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[0]);

      if (pixelPosInItem[0])
        // If the zoom box is active, draw a rect around the pixel currently under the cursor
        painter.drawRect( zoomPixelRect[0] );

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(0, &painter, xSplit, drawArea_botR, itemZoomBoxTranslation[0], item[0], frame, pixelPos[0], pixelPosInItem[0] );
    }
    if (item[1])
    {
      // Set clipping to the right region
      QRegion clip = QRegion(xSplit, 0, drawArea_botR.x() - xSplit, drawArea_botR.y());
      painter.setClipRegion( clip );

      // Translate the painter to the position where we want the item to be
      painter.translate( centerPoints[1] + centerOffset );

      // Draw the item at position (0,0)
      item[1]->drawFrame( &painter, frame, zoomFactor );

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[1]);

      if (pixelPosInItem[1])
        // If the zoom box is active, draw a rect around the pixel currently under the cursor
        painter.drawRect( zoomPixelRect[1] );

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(1, &painter, xSplit, drawArea_botR, itemZoomBoxTranslation[1], item[1], frame, pixelPos[1], pixelPosInItem[1] );
    }

    // Disable clipping
    painter.setClipping( false );
  }
  else // (!splitting)
  {
    // Draw one item (if one item is selected)
    if (item[0])
    {
      centerPoints[0] = drawArea_botR / 2;

      // Translate the painter to the position where we want the item to be
      painter.translate( centerPoints[0] + centerOffset );

      // Draw the item at position (0,0). 
      item[0]->drawFrame( &painter, frame, zoomFactor );

      // Paint the regular gird
      if (drawRegularGrid)
        paintRegularGrid(&painter, item[0]);

      if (pixelPosInItem[0])
        // If the zoom box is active, draw a rect around the pixel currently under the cursor
        painter.drawRect( zoomPixelRect[0] );

      // Do the inverse translation of the painter
      painter.resetTransform();

      // Paint the zoom box for view 0
      paintZoomBox(0, &painter, xSplit, drawArea_botR, itemZoomBoxTranslation[0], item[0], frame, pixelPos[0], pixelPosInItem[0] );
    }
  }
  
  if (splitting)
  {
    if (splittingLineStyle == TOP_BOTTOM_HANDLERS)
    {
      // Draw small handlers at the top and bottom
      QPainterPath triangle;
      triangle.moveTo( xSplit-10, 0 );
      triangle.lineTo( xSplit   , 10);
      triangle.lineTo( xSplit+10,  0);
      triangle.closeSubpath();

      triangle.moveTo( xSplit-10, drawArea_botR.y() );
      triangle.lineTo( xSplit   , drawArea_botR.y() - 10);
      triangle.lineTo( xSplit+10, drawArea_botR.y() );
      triangle.closeSubpath();

      painter.fillPath( triangle, Qt::white );
    }
    else
    {
      // Draw the splitting line at position xSplit. All pixels left of the line
      // belong to the left view, and all pixels on the right belong to the right one.
      QLine line(xSplit, 0, xSplit, drawArea_botR.y());
      QPen splitterPen(Qt::white);
      //splitterPen.setStyle(Qt::DashLine);
      painter.setPen(splitterPen);
      painter.drawLine(line);
    }
  }

  // Draw the zoom factor
  if (zoomFactor != 1.0)
  {
    QString zoomString = QString("x%1").arg(zoomFactor);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QColor(Qt::black));
    painter.setFont(zoomFactorFont);
    painter.drawText(zoomFactorFontPos, zoomString);
  }
}

void splitViewWidget::paintZoomBox(int view, QPainter *painter, int xSplit, QPoint drawArea_botR, QPointF itemZoomBoxTranslation, playlistItem *item, int frame, QPoint pixelPos, bool pixelPosInItem)
{
  if (!drawZoomBox)
    return;

  const int zoomBoxFactor = 32;
  const int srcSize = 5;
  const int targetSizeHalf = srcSize*zoomBoxFactor/2;
  const int margin = 11;
  const int padding = 6;

  // Where will the zoom view go?
  QRect zoomViewRect(0,0, targetSizeHalf*2, targetSizeHalf*2);

  bool drawInfoPanel = true;  // Do we draw the info panel?
  if (view == 1 && xSplit > (drawArea_botR.x() - margin - targetSizeHalf * 2))
  {
    if (xSplit > drawArea_botR.x() - margin)
      // The split line is so far on the right, that the whole zoom box in view 1 is not visible
      return;

    // The split line is so far right, that part of the zoom box is hidden.
    // Resize the zoomViewRect to the part that is visible.
    zoomViewRect.setWidth( drawArea_botR.x() - xSplit - margin );
    
    drawInfoPanel = false;  // Info panel not visible
  }

  //
  if (view == 0 && splitting)
    zoomViewRect.moveBottomRight( QPoint(xSplit - margin, drawArea_botR.y() - margin) );
  else
    zoomViewRect.moveBottomRight( drawArea_botR - QPoint(margin, margin) );

  // Fill the viewRect with the background color
  painter->fillRect(zoomViewRect, painter->background());
  
  // Restrict drawing to the zoom view rect. Save the old clipping region (if any) so we can
  // reset it later
  QRegion clipRegion;
  if (painter->hasClipping())
    clipRegion = painter->clipRegion();
  painter->setClipRegion( zoomViewRect );

  // Translate the painter to the point where the center of the zoom view will be
  painter->translate( zoomViewRect.center() );

  // Now we have to calculate the translation of the item, so that the pixel position
  // is in the center of the view (so we can draw it at (0,0)).
  painter->translate( itemZoomBoxTranslation * zoomBoxFactor );

  // Draw the item again, but this time with a high zoom factor into the clipped region
  item->drawFrame( painter, frame, zoomBoxFactor );

  // Reset transform and reset clipping to the previous clip region (if there was one)
  painter->resetTransform();
  if (clipRegion.isEmpty())
    painter->setClipping(false);
  else
    painter->setClipRegion(clipRegion);

  // Draw a rect around the zoom view
  painter->drawRect(zoomViewRect);

  if (drawInfoPanel)
  {
    // Draw pixel info. First, construct the text and see how the size is going to be.
    QString pixelInfoString = QString("<h4>Coordinates</h4>"
                              "<table width=\"100%\">"
                              "<tr><td>X:</td><td align=\"right\">%1</td></tr>"
                              "<tr><td>Y:</td><td align=\"right\">%2</td></tr>"
                              "</table>"
                              ).arg(pixelPos.x()).arg(pixelPos.y());

    // If the pixel position is within the item, append information on the pixel vale
    if (pixelPosInItem)
    {
      ValuePairList pixelValues = item->getPixelValues( pixelPos );
      // if we have some values, show them
      if( pixelValues.size() > 0 )
      {
        pixelInfoString.append( QString("<h4>%1</h4>"
                                "<table width=\"100%\">").arg(pixelValues.title) );
        for (int i = 0; i < pixelValues.size(); ++i)
          pixelInfoString.append( QString("<tr><td><nobr>%1:</nobr></td><td align=\"right\"><nobr>%2</nobr></td></tr>").arg(pixelValues[i].first).arg(pixelValues[i].second) );
        pixelInfoString.append( "</table>" );
      }
    }

    // Create a QTextDocument. This object can tell us the size of the rendered text.
    QTextDocument textDocument;
    textDocument.setDefaultStyleSheet("* { color: #FFFFFF }");
    textDocument.setHtml(pixelInfoString);
    textDocument.setTextWidth(textDocument.size().width());

    // Translate to the position where the text box shall be
    if (view == 0 && splitting)
      painter->translate(xSplit - margin - targetSizeHalf*2 - textDocument.size().width() - padding*2 + 1, drawArea_botR.y() - margin - textDocument.size().height() - padding*2 + 1);
    else
      painter->translate(drawArea_botR.x() - margin - targetSizeHalf*2 - textDocument.size().width() - padding*2 + 1, drawArea_botR.y() - margin - textDocument.size().height() - padding*2 + 1);

    // Draw a black rect and then the text on top of that
    QRect rect(QPoint(0, 0), textDocument.size().toSize() + QSize(2*padding, 2*padding));
    QBrush originalBrush;
    painter->setBrush(QColor(0, 0, 0, 70));
    painter->drawRect(rect);
    painter->translate(padding, padding);
    textDocument.drawContents(painter);
    painter->setBrush(originalBrush);

    painter->resetTransform();
  }
}

void splitViewWidget::paintRegularGrid(QPainter *painter, playlistItem *item)
{
  QSize itemSize = item->getVideoSize() * zoomFactor;

  // Draw horizontal lines
  const int xMin = -itemSize.width() / 2;
  const int xMax =  itemSize.width() / 2;
  const int gridZoom = regularGridSize * zoomFactor;
  for (int y = 1; y <= (itemSize.height() - 1) / gridZoom; y++)
  {
    int yPos = (-itemSize.height() / 2) + y * gridZoom;
    painter->drawLine(xMin, yPos, xMax, yPos);
  }

  // Draw vertical lines
  const int yMin = -itemSize.height() / 2;
  const int yMax =  itemSize.height() / 2;
  for (int x = 1; x <= (itemSize.width() - 1) / gridZoom; x++)
  {
    int xPos = (-itemSize.width() / 2) + x * gridZoom;
    painter->drawLine(xPos, yMin, xPos, yMax);
  }
}

void splitViewWidget::mouseMoveEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::NoButton)
  {
    // We want this event
    mouse_event->accept();

    if (splitting && splittingDragging)
    {
      // The user is currently dragging the splitter. Calculate the new splitter point.
      int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2- SPLITVIEWWIDGET_SPLITTER_CLIPX));
      splittingPoint = (double)xClip / (double)(width()-2);

      // The splitter was moved. Update the widget.
      update();
    }
    else if (viewDragging)
    {
      // The user is currently dragging the view. Calculate the new offset from the center position
      centerOffset = viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart);

      // The view was moved. Update the widget.
      update();
    }
    else if (splitting)
    {
      // No buttons pressed, the view is split and we are not dragging.
      int splitPosPix = int((width()-2) * splittingPoint);

      if (mouse_event->x() > (splitPosPix-SPLITVIEWWIDGET_SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITVIEWWIDGET_SPLITTER_MARGIN)) 
      {
        // Mouse is over the line in the middle (plus minus 4 pixels)
        setCursor(Qt::SplitHCursor);
      }
      else 
      {
        // Mouse is not over the splitter line
        setCursor(Qt::ArrowCursor);
      }
    }
  }

  if (drawZoomBox)
  {
    // If the mouse position changed, save the current point of the mouse and update the view (this will update the zoom box)
    if (zoomBoxMousePosition != mouse_event->pos())
    {
      zoomBoxMousePosition = mouse_event->pos();
      update();
    }
  }
}

void splitViewWidget::mousePressEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::LeftButton)
  {
    // Left mouse buttons pressed
    int splitPosPix = int((width()-2) * splittingPoint);

    // TODO: plus minus 4 pixels for the handle might be way to little for high DPI displays. This should depend on the screens DPI.
    if (splitting && mouse_event->x() > (splitPosPix-SPLITVIEWWIDGET_SPLITTER_MARGIN) && mouse_event->x() < (splitPosPix+SPLITVIEWWIDGET_SPLITTER_MARGIN)) 
    {
      // Mouse is over the splitter. Activate dragging of splitter.
      splittingDragging = true;

      // We handeled this event
      mouse_event->accept();
    }
  }
  else if (mouse_event->button() == Qt::RightButton)
  {
    // The user pressed the right mouse button. In this case drag the view.
    viewDragging = true;

    // Reset the cursor if it was another cursor (over the splitting line for example)
    setCursor(Qt::ArrowCursor);

    // Save the position where the user grabbed the item (screen), and the current value of 
    // the centerOffset. So when the user moves the mouse, the new offset is just the old one
    // plus the difference between the position of the mouse and the position where the
    // user grabbed the item (screen).
    viewDraggingMousePosStart = mouse_event->pos();
    viewDraggingStartOffset = centerOffset;
      
    //qDebug() << "MouseGrab - Center: " << centerPoint << " rel: " << grabPosRelative;
      
    // We handeled this event
    mouse_event->accept();
  }
}

void splitViewWidget::mouseReleaseEvent(QMouseEvent *mouse_event)
{
  if (mouse_event->button() == Qt::LeftButton && splitting && splittingDragging) 
  {
    // We want this event
    mouse_event->accept();

    // The left mouse button was released, we are showing a split view and the user is dragging the splitter.
    // End splitting.

    // Update current splitting position / update last time
    int xClip = clip(mouse_event->x(), SPLITVIEWWIDGET_SPLITTER_CLIPX, (width()-2-SPLITVIEWWIDGET_SPLITTER_CLIPX));
    splittingPoint = (double)xClip / (double)(width()-2);
    update();

    splittingDragging = false;
  }
  else if (mouse_event->button() == Qt::RightButton && viewDragging)
  {
    // We want this event
    mouse_event->accept();

    // Calculate the new center offset one last time
    centerOffset = viewDraggingStartOffset + (mouse_event->pos() - viewDraggingMousePosStart);

    // The view was moved. Update the widget.
    update();

    // End dragging
    viewDragging = false;
  }
}

void splitViewWidget::wheelEvent (QWheelEvent *e)
{
  QPoint p = e->pos();
  e->accept();
  if (e->delta() > 0)
  {
    zoomIn(p);
  }
  else
  {
    zoomOut(p);
  }
}

void splitViewWidget::zoomIn(QPoint zoomPoint)
{ 
  // The zoom point works like this: After the zoom operation the pixel at zoomPoint shall
  // still be at the same position (zoomPoint)

  if (!zoomPoint.isNull())
  {
    // The center point has to be moved relative to the zoomPoint

    // Get the absolute center point of the item
    QPoint drawArea_botR(width(), height());
    QPoint centerPoint = drawArea_botR / 2;

    if (splitting && viewMode == SIDE_BY_SIDE)
    {
      // For side by side mode, the center points are centered in each individual split view

      // Which side of the split view are we zooming in?
      // Get the center point of that view
      int xSplit = int(drawArea_botR.x() * splittingPoint);
      if (zoomPoint.x() > xSplit)
        // Zooming in the right view
        centerPoint = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2 );
      else
        // Tooming in the left view
        centerPoint = QPoint( xSplit / 2, drawArea_botR.y() / 2 );
    }
    
    // The absolute center point of the item under the cursor
    QPoint itemCenter = centerPoint + centerOffset;

    // Move this item center point
    QPoint diff = itemCenter - zoomPoint;
    diff *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
    itemCenter = zoomPoint + diff;

    // Calculate the new cente offset
    centerOffset = itemCenter - centerPoint;
  }
  else
  {
    // Zoom in without considering the mouse position
    centerOffset *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }

  zoomFactor *= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR; 
  update(); 
}

void splitViewWidget::zoomOut(QPoint zoomPoint) 
{ 
  if (!zoomPoint.isNull() && SPLITVIEWWIDGET_ZOOM_OUT_MOUSE == 1)
  {
    // The center point has to be moved relative to the zoomPoint

    // Get the absolute center point of the item
    QPoint drawArea_botR(width(), height());
    QPoint centerPoint = drawArea_botR / 2;

    if (splitting && viewMode == SIDE_BY_SIDE)
    {
      // For side by side mode, the center points are centered in each individual split view

      // Which side of the split view are we zooming in?
      // Get the center point of that view
      int xSplit = int(drawArea_botR.x() * splittingPoint);
      if (zoomPoint.x() > xSplit)
        // Zooming in the right view
        centerPoint = QPoint( xSplit + (drawArea_botR.x() - xSplit) / 2, drawArea_botR.y() / 2 );
      else
        // Tooming in the left view
        centerPoint = QPoint( xSplit / 2, drawArea_botR.y() / 2 );
    }
    
    // The absolute center point of the item under the cursor
    QPoint itemCenter = centerPoint + centerOffset;

    // Move this item center point
    QPoint diff = itemCenter - zoomPoint;
    diff /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
    itemCenter = zoomPoint + diff;

    // Calculate the new cente offset
    centerOffset = itemCenter - centerPoint;
  }
  else
  {
    // Zoom out without considering the mouse position. 
    centerOffset /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  }
    
  zoomFactor /= SPLITVIEWWIDGET_ZOOM_STEP_FACTOR;
  update();
}

void splitViewWidget::resetViews()
{
  centerOffset = QPoint(0,0);
  zoomFactor = 1.0;
  splittingPoint = 0.5;

  update();
}

void splitViewWidget::setuptControls(QDockWidget *dock)
{
  // Initialize the controls and add them to the given widget.
  QWidget *controlsWidget = new QWidget(dock);
  controls->setupUi( controlsWidget );
  dock->setWidget( controlsWidget );

  // Connect signals/slots
  connect(controls->SplitViewgroupBox, SIGNAL(toggled(bool)), this, SLOT(on_SplitViewgroupBox_toggled(bool)));
  connect(controls->viewComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(on_viewComboBox_currentIndexChanged(int)));
  connect(controls->regularGridCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_regularGridCheckBox_toggled(bool)));
  connect(controls->gridSizeBox, SIGNAL(valueChanged(int)), this, SLOT(on_gridSizeBox_valueChanged(int)));
  connect(controls->zoomBoxCheckBox, SIGNAL(toggled(bool)), this, SLOT(on_zoomBoxCheckBox_toggled(bool)));
}

void splitViewWidget::on_viewComboBox_currentIndexChanged(int index)
{
  switch (index)
  {
    case 0: // SIDE_BY_SIDE
      setViewMode(SIDE_BY_SIDE);
      break;
    case 1: // COMPARISON
      setViewMode(COMPARISON);
      break;
  }
}

