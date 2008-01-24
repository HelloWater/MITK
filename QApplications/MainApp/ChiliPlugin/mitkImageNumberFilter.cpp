/*=========================================================================

Program:   Medical Imaging & Interaction Toolkit
Language:  C++
Date:      $Date: 2007-06-15 14:28:00 +0200 (Fr, 15 Jun 2007) $
Version:   $Revision: 10778 $

Copyright (c) German Cancer Research Center, Division of Medical and
Biological Informatics. All rights reserved.
See MITKCopyright.txt or http://www.mitk.org/copyright.html for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notices for more information.

=========================================================================*/

#include "mitkImageNumberFilter.h"

// Chili-Includes
#include <chili/isg.h>
#include <chili/plugin.h>
#include <ipPic/ipPicTags.h>
#include <ipDicom/ipDicom.h>
// MITK-Includes
#include "mitkProperties.h"
#include "mitkPlaneGeometry.h"
#include "mitkFrameOfReferenceUIDManager.h"
#include "mitkDataTreeNodeFactory.h"
#include "mitkChiliMacros.h"
#include "math.h"

// helper class to sort PicDescriptors primary by there imageNumber; if the imageNumber equal, the sliceLocation get used
class compare_PicDescriptor_ImageNumber
{
  public:
  bool operator() ( ipPicDescriptor* &first, ipPicDescriptor* &second)
  {
    int imageNumberPic1 = 0, imageNumberPic2 = 0;

    ipPicTSV_t* imagenumberTag1 = ipPicQueryTag( first, (char*)tagIMAGE_NUMBER );
    if( imagenumberTag1 && imagenumberTag1->type == ipPicInt )
    {
      imageNumberPic1 = *( (int*)(imagenumberTag1->value) );
    }
    else
    {
      ipPicTSV_t *tsv;
      void* data = NULL;
      ipUInt4_t len = 0;
      tsv = ipPicQueryTag( first, (char*)"SOURCE HEADER" );
      if( tsv )
      {
        if( dicomFindElement( (unsigned char*) tsv->value, 0x0020, 0x0013, &data, &len ) )
        {
          sscanf( (char *) data, "%d", &imageNumberPic1 );
        }
      }
    }

    ipPicTSV_t* imagenumberTag2 = ipPicQueryTag( second, (char*)tagIMAGE_NUMBER );
    if( imagenumberTag2 && imagenumberTag2->type == ipPicInt )
    {
      imageNumberPic2 = *( (int*)(imagenumberTag2->value) );
    }
    else
    {
      ipPicTSV_t *tsv;
      void* data = NULL;
      ipUInt4_t len = 0;
      tsv = ipPicQueryTag( second, (char*)"SOURCE HEADER" );
      if( tsv )
      {
        if( dicomFindElement( (unsigned char*) tsv->value, 0x0020, 0x0013, &data, &len ) )
        {
          sscanf( (char *) data, "%d", &imageNumberPic2 );
        }
      }
    }
#ifdef CHILI_PLUGIN_VERSION_CODE
    if( imageNumberPic1 == imageNumberPic2 )
    {
      bool returnValue = false;
      interSliceGeometry_t* isg1 = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
      interSliceGeometry_t* isg2 = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
      if( pFetchSliceGeometryFromPic( first, isg1 ) && pFetchSliceGeometryFromPic( second, isg2 ) )
      {
        if( isg1->sl < isg2->sl )
        {
          returnValue = true;
        }
      }
      delete isg1;
      delete isg2;
      return returnValue;
    }
    else
#endif
    {
      if( imageNumberPic1 < imageNumberPic2 )
      {
        return true;
      }
      else
      {
        return false;
      }
    }
  }
};

// helper class to sort PicDescriptors primary by there sliceLocation
class compare_PicDescriptor_SliceLocation
{
  public:
  bool operator() ( ipPicDescriptor*& mitkHideIfNoVersionCode( first ) , ipPicDescriptor*& mitkHideIfNoVersionCode( second ) )
  {
    bool returnValue = false;
#ifdef CHILI_PLUGIN_VERSION_CODE
    interSliceGeometry_t* isg1 = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
    interSliceGeometry_t* isg2 = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
    if( pFetchSliceGeometryFromPic( first, isg1 ) && pFetchSliceGeometryFromPic( second, isg2 ) )
    {
      if( isg1->sl < isg2->sl )
      {
        returnValue = true;
      }
    }
    delete isg1;
    delete isg2;
#endif
    return returnValue;
  }
};

// helper class to round
double mitk::ImageNumberFilter::Round(double number, unsigned int decimalPlaces)
{
  double d = pow( (long double)10.0, (int)decimalPlaces );
  double x;
  if( number > 0 )
    x = floor( number * d + 0.5 ) / d;
  else
    x = floor( number * d - 0.5 ) / d;
  return x;
}

// constructor
mitk::ImageNumberFilter::ImageNumberFilter()
{
}

// destructor
mitk::ImageNumberFilter::~ImageNumberFilter()
{
}

// set-function
void mitk::ImageNumberFilter::SetInput( std::list< ipPicDescriptor* > inputPicDescriptorList, std::string inputSeriesOID )
{
  m_SeriesOID = inputSeriesOID;
  m_PicDescriptorList = inputPicDescriptorList;
}

// get-function
std::vector< mitk::DataTreeNode::Pointer > mitk::ImageNumberFilter::GetOutput()
{
  return m_Output;
}

// the "main"-function
void mitk::ImageNumberFilter::Update()
{
  m_Output.clear();
  m_PossibleOutputs.clear();

  if( m_PicDescriptorList.size() > 0 && m_SeriesOID != "" )
  {
    CreatePossibleOutputs();
    SortByImageNumber();
    SeperateOutputsBySpacing();
    SeperateOutputsByTime();
    SplitDummiVolumes();
    SortBySliceLocation();
    CreateNodesFromOutputs();
    //DebugOutput();
  }
  else std::cout<<"ImageNumberFilter-WARNING: No SeriesOID or PicDescriptorList set."<<std::endl;
}

void mitk::ImageNumberFilter::CreatePossibleOutputs()
{
#ifdef CHILI_PLUGIN_VERSION_CODE
  // all slices without isg get ignored
  // sort by frameOfRefferenceUID --> all slices of one geometry have the same one
  // sort by "NormalenVektor" --> if they are parallel, the slices located one above the other
  // sort by pixelSpacing --> separation of extent
  // sort by SeriesDescription
  for( std::list< ipPicDescriptor* >::iterator currentPic = m_PicDescriptorList.begin(); currentPic != m_PicDescriptorList.end(); currentPic ++ )
  {
    Vector3D rightVector, downVector, pixelSize, normale;
    std::string currentSeriesDescription = "";
    bool foundMatch; // true if the searched output exist
    int maxCount, curCount; // parameter to searching outputs

    //check intersliceGeomtry
    interSliceGeometry_t* isg = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
    if( !pFetchSliceGeometryFromPic( (*currentPic), isg ) )
    {
      //PicDescriptor without a geometry not able to sort in a volume
      std::cout<<"ImageNumberFilter-WARNING: Found image without SliceGeometry. Image ignored."<<std::endl;
      delete isg;
      continue;
    }

    //"Normal-Vektor"
    vtk2itk( isg->u, rightVector );
    vtk2itk( isg->v, downVector );
    rightVector = rightVector * (*currentPic)->n[0];
    downVector = downVector * (*currentPic)->n[1];
    normale[0] = Round( ( ( rightVector[1]*downVector[2] ) - ( rightVector[2]*downVector[1] ) ), 2 );
    normale[1] = Round( ( ( rightVector[2]*downVector[0] ) - ( rightVector[0]*downVector[2] ) ), 2);
    normale[2] = Round( ( ( rightVector[0]*downVector[1] ) - ( rightVector[1]*downVector[0] ) ), 2);

    //PixelSize
    vtk2itk( isg->ps, pixelSize );

    //SeriesDescription
    ipPicTSV_t* seriesDescriptionTag = ipPicQueryTag( (*currentPic), (char*)tagSERIES_DESCRIPTION );
    if( seriesDescriptionTag )
    {
      currentSeriesDescription = static_cast<char*>( seriesDescriptionTag->value );
    }
    if( currentSeriesDescription == "" )
    {
      ipPicTSV_t *tsv;
      void* data = NULL;
      ipUInt4_t len = 0;
      tsv = ipPicQueryTag( (*currentPic), (char*)"SOURCE HEADER" );
      if( tsv )
      {
        if( dicomFindElement( (unsigned char*) tsv->value, 0x0008, 0x103e, &data, &len ) )
        {
          currentSeriesDescription = (char*)data;
        }
      }
    }

    //dimension
    int currentDimension = (*currentPic)->dim;
    if( currentDimension < 2 || currentDimension > 4 )
    {
      std::cout<<"ImageNumberFilter-WARNING: Wrong PicDescriptor-Dimension. Image ignored."<<std::endl;
      delete isg;
      continue;
    }

    //origin
    Vector3D currentOrigin;
    vtk2itk( isg->o, currentOrigin );

    if( currentDimension == 4 )
    {
      //with this combination, no search initialize and a new group created
      foundMatch = false;
      curCount = 0;
      maxCount = 0;
    }
    else
    {
      //initialize searching
      foundMatch = false;
      curCount = 0;
      maxCount = m_PossibleOutputs.size();
    }

    // searching for equal output
    while( curCount < maxCount && !foundMatch )
    {
      //check RefferenceUID, PixelSize, Expansion and SeriesDescription
      if( isg->forUID == m_PossibleOutputs[ curCount ].refferenceUID && Equal(pixelSize, m_PossibleOutputs[ curCount ].pixelSize) && currentSeriesDescription == m_PossibleOutputs[ curCount ].seriesDescription && currentDimension == m_PossibleOutputs[ curCount ].dimension )
      {
        //check if vectors are parallel (only if they have a lowest common multiple)
        foundMatch = true; // --> found the right output
        float referenceForParallelism = 0;
        //create reference
        if( normale[0] != m_PossibleOutputs[ curCount ].normale[0] )
          referenceForParallelism = normale[0]/m_PossibleOutputs[ curCount ].normale[0];
        //check the second
        if( normale[1] != m_PossibleOutputs[ curCount ].normale[1] )
        {
          if( referenceForParallelism == 0 )
            referenceForParallelism = normale[1]/m_PossibleOutputs[ curCount ].normale[1];
          else
          {
            if( referenceForParallelism != normale[1]/m_PossibleOutputs[ curCount ].normale[1] )
              foundMatch = false;  // --> not parallel, wrong output
          }
        }
        //check the third
        if( normale[2] != m_PossibleOutputs[ curCount ].normale[2] )
        {
          if( referenceForParallelism == 0 )
            referenceForParallelism = normale[2]/m_PossibleOutputs[ curCount ].normale[2];
          else
          {
            if( referenceForParallelism != normale[2]/m_PossibleOutputs[ curCount ].normale[2] )
              foundMatch = false;  // --> not parallel, wrong output
          }
        }
        //now we know about the dimension, normale, referenceUID, pixelSize and description
        //but there are dimension specific cases
        if( m_PossibleOutputs[ curCount ].dimension == 2 )
        {
          if( !foundMatch )
            curCount ++;
        }
        else
        {
          if( foundMatch && ( m_PossibleOutputs[ curCount ].origin != currentOrigin || m_PossibleOutputs[ curCount ].numberOfSlices != (int)(*currentPic)->n[2] ) )
          {
            foundMatch = false;
            curCount++;
          }
          else curCount++;
        }
      }
      else curCount++;
    }

    // match found?
    if( foundMatch )
    {
      // add to an existing Output
      m_PossibleOutputs[curCount].descriptors.push_back( (*currentPic) );
    }
    else
    // create a new Output
    {
      DifferentOutputs newOutput;
      newOutput.refferenceUID = isg->forUID;
      newOutput.seriesDescription = currentSeriesDescription;
      newOutput.normale = normale;
      newOutput.pixelSize = pixelSize;
      newOutput.dimension = currentDimension;
      newOutput.origin = currentOrigin;
      if( currentDimension > 2 )
        newOutput.numberOfSlices = (*currentPic)->n[2]-1;
      else
        newOutput.numberOfSlices = -1;  //from here on only defaults. True values will be calculated later
      if( currentDimension == 4 )
        newOutput.numberOfTimeSlices = (*currentPic)->n[3]-1;
      else
        newOutput.numberOfTimeSlices = -1;
      newOutput.differentTimeSlices = false;
      newOutput.descriptors.clear();
      newOutput.descriptors.push_back( (*currentPic) );
      m_PossibleOutputs.push_back( newOutput );
    }
    delete isg;
  }
#endif
}

//sort all possible outputs by imagenumber
void mitk::ImageNumberFilter::SortByImageNumber()
{
  for( unsigned int n = 0; n < m_PossibleOutputs.size(); n++)
  {
    compare_PicDescriptor_ImageNumber cPI;
    m_PossibleOutputs[n].descriptors.sort( cPI );
  }
}

//sort all possible outputs by slicelocation
void mitk::ImageNumberFilter::SortBySliceLocation()
{
  for( unsigned int n = 0; n < m_PossibleOutputs.size(); n++)
  {
    compare_PicDescriptor_SliceLocation cPS;
    m_PossibleOutputs[n].descriptors.sort( cPS );
  }
}

// separation on spacing and set minimum of timslices and slices
void mitk::ImageNumberFilter::SeperateOutputsBySpacing()
{
#ifdef CHILI_PLUGIN_VERSION_CODE
  for( unsigned int n = 0; n < m_PossibleOutputs.size(); n++)
  {
    if( m_PossibleOutputs[n].dimension == 3 )
    {
      m_PossibleOutputs[n].numberOfTimeSlices = m_PossibleOutputs[n].descriptors.size()-1;
      continue;
    }
    if( m_PossibleOutputs[n].dimension == 4 )
      continue;

    std::list< ipPicDescriptor* >::iterator iter = m_PossibleOutputs[n].descriptors.begin();
    std::list< ipPicDescriptor* >::iterator iterend = m_PossibleOutputs[n].descriptors.end();
    bool InitializedSpacingAndTime = false;
    int numberOfSlices = 0, currentCount = 0, numberOfTimeSlices = 0;
    interSliceGeometry_t* isg;
    Vector3D spacing, tempSpacing, origincur, originb4;
    //end() return a iterator after the last element, but we want the last element
    iterend--;

    //set the default-spacing to output
    isg = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
    if( !pFetchSliceGeometryFromPic( (*iter), isg ) )
    {
      delete isg;
      return;
    }

    vtk2itk( isg->ps, spacing );
    if( spacing[0] == 0 && spacing[1] == 0 && spacing[2] == 0 )
      spacing.Fill(1.0);
    for (unsigned int i = 0; i < 3; ++i)
      spacing[i] = Round( spacing[i], 2 );
    m_PossibleOutputs[n].sliceSpacing = spacing;

    //start checking for spacing if the output contain more than one slice
    if( m_PossibleOutputs[n].descriptors.size() > 1 )
    {
      vtk2itk( isg->o, origincur );

      while( iter != iterend )
      {
        originb4 = origincur;
        iter ++;
        currentCount ++;

        if( !pFetchSliceGeometryFromPic( (*iter), isg ) )
        {
          delete isg;
          return;
        }

        vtk2itk( isg->o, origincur );

        // they are equal
        if( Equal( originb4, origincur ) )
          numberOfTimeSlices ++;
        else
        {
          // they are different
          tempSpacing = origincur - originb4;
          spacing[2] = tempSpacing.GetNorm();
          spacing[2] = Round( spacing[2], 2 );

          // spacing and timeslices not initialized yet
          if( !InitializedSpacingAndTime )
          {
            numberOfSlices ++;
            InitializedSpacingAndTime = true;
            m_PossibleOutputs[n].numberOfTimeSlices = numberOfTimeSlices;
            m_PossibleOutputs[n].sliceSpacing = spacing;
            numberOfTimeSlices = 0;
          }
          else
          // spacing and timeslices initialized
          {
            if( spacing == m_PossibleOutputs[n].sliceSpacing )
            {
              numberOfSlices ++;
              if( m_PossibleOutputs[n].numberOfTimeSlices != numberOfTimeSlices )
              {
                m_PossibleOutputs[n].differentTimeSlices = true;
                if( m_PossibleOutputs[n].numberOfTimeSlices > numberOfTimeSlices )
                  m_PossibleOutputs[n].numberOfTimeSlices = numberOfTimeSlices;
              }
              numberOfTimeSlices = 0;
            }
            else
            {
              // the current found spacing get illustrated in one Output, the remaining slices get relocated in a new Output and checked the same way later
              DifferentOutputs newOutput;
              newOutput.refferenceUID = m_PossibleOutputs[n].refferenceUID;
              newOutput.seriesDescription = m_PossibleOutputs[n].seriesDescription;
              newOutput.normale = m_PossibleOutputs[n].normale;
              newOutput.pixelSize = m_PossibleOutputs[n].pixelSize;
              newOutput.dimension = m_PossibleOutputs[n].dimension;
              newOutput.origin = m_PossibleOutputs[n].origin;
              newOutput.numberOfSlices = - 1;
              newOutput.numberOfTimeSlices = - 1;
              newOutput.differentTimeSlices = false;
              newOutput.descriptors.clear();
              // we dont want to cut until the last element, we cut after the last element
              iterend ++;
              newOutput.descriptors.assign( iter, iterend );
              iterend --;
              m_PossibleOutputs[n].descriptors.resize( currentCount );
              m_PossibleOutputs.push_back( newOutput );
              iterend = iter;  // --> end searching the current output
            }
          }
        }
      }
    }
    m_PossibleOutputs[n].numberOfSlices = numberOfSlices;

    //check last slice for time
    if( m_PossibleOutputs[n].numberOfTimeSlices != numberOfTimeSlices )
    {
      m_PossibleOutputs[n].differentTimeSlices = true;
      if( numberOfTimeSlices < m_PossibleOutputs[n].numberOfTimeSlices )
        m_PossibleOutputs[n].numberOfTimeSlices = numberOfTimeSlices;
    }

    // 2D-Dataset or timesliced 2D-Dataset
    if( m_PossibleOutputs[n].descriptors.size() == 1 || !InitializedSpacingAndTime )
    {
      m_PossibleOutputs[n].sliceSpacing = spacing;
      m_PossibleOutputs[n].numberOfTimeSlices = numberOfTimeSlices;
    }
    delete isg;
  }
#endif
}

//separate the PossibleOutputs by time
void mitk::ImageNumberFilter::SeperateOutputsByTime()
{
#ifdef CHILI_PLUGIN_VERSION_CODE
  for( unsigned int n = 0; n < m_PossibleOutputs.size(); n++)
  {
    if( m_PossibleOutputs[n].differentTimeSlices && m_PossibleOutputs[n].dimension == 2 )
    {
      int curTime = 0, lastTime = 0;
      bool deleteIterator = false;
      Vector3D origincur, originb4;

      DifferentOutputs* timeOutput = NULL;

      interSliceGeometry_t* isg = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
      if( !pFetchSliceGeometryFromPic( (*m_PossibleOutputs[n].descriptors.begin()), isg ) )
      {
        delete isg;
        return;
      }

      vtk2itk( isg->o, origincur );

      std::list< ipPicDescriptor* >::iterator iter = m_PossibleOutputs[n].descriptors.begin();
      std::list< ipPicDescriptor* >::iterator iterend = m_PossibleOutputs[n].descriptors.end();
      iterend--;

      while( iter != iterend )
      {
        if( deleteIterator )
        //the ipPicDescriptor was saved to "timeOuput", so we can delete them from the current Output
        {
          iter = m_PossibleOutputs[n].descriptors.erase( iter );
          deleteIterator = false;
        }
        else
        {
          iter ++;
          originb4 = origincur;
        }

        //get the current origin
        if( !pFetchSliceGeometryFromPic( (*iter), isg ) )
        {
          delete isg;
          return;
        }

        vtk2itk( isg->o, origincur );

        //check them
        if( Equal( originb4, origincur ) )
        {
          curTime ++;
          if( curTime > m_PossibleOutputs[n].numberOfTimeSlices )
          {
            deleteIterator = true;
            if( timeOutput == NULL )
            {
              //create a "new" Output
              timeOutput = new DifferentOutputs;
              timeOutput->refferenceUID = m_PossibleOutputs[n].refferenceUID;
              timeOutput->seriesDescription = m_PossibleOutputs[n].seriesDescription;
              timeOutput->normale = m_PossibleOutputs[n].normale;
              timeOutput->pixelSize = m_PossibleOutputs[n].pixelSize;
              timeOutput->sliceSpacing = m_PossibleOutputs[n].sliceSpacing;
              timeOutput->dimension = m_PossibleOutputs[n].dimension;
              timeOutput->origin = m_PossibleOutputs[n].origin;
              timeOutput->numberOfSlices = - 1;
              timeOutput->numberOfTimeSlices = - 1;
              timeOutput->differentTimeSlices = false;
              timeOutput->descriptors.clear();
              timeOutput->descriptors.push_back( (*iter) );
            }
            else
            {
              //add to output
              timeOutput->descriptors.push_back( (*iter) );
            }
          }
        }
        else
        //they are not equal
        {
          if( curTime > m_PossibleOutputs[n].numberOfTimeSlices )
          //set slice and timeslice
          {
            timeOutput->numberOfSlices ++;
            //if the numberOfTimeSlices not initialiced and the possibleOutputs contain only one picDescriptor
            if( timeOutput->numberOfTimeSlices == -1 )
              //intialice the numberOfTimeSlices
              timeOutput->numberOfTimeSlices = ( curTime - m_PossibleOutputs[n].numberOfTimeSlices - 1 );
            else
            {
              if( curTime < timeOutput->numberOfTimeSlices )
              {
                timeOutput->numberOfTimeSlices = ( curTime - m_PossibleOutputs[n].numberOfTimeSlices - 1 );
                timeOutput->differentTimeSlices = true;
              }
              else
              {
                if( curTime > timeOutput->numberOfTimeSlices )
                  timeOutput->differentTimeSlices = true;
              }
            }
          }
          if( curTime == m_PossibleOutputs[n].numberOfTimeSlices && timeOutput != NULL )
          {
            // "close" output
            // its possible that "push_back" have to increase the capacity, then all iterator become void
            // so we have to set the iterator new
            ipPicDescriptor* tempDescriptor = (*iter);
            m_PossibleOutputs.push_back( (*timeOutput) );
            iter = m_PossibleOutputs[n].descriptors.begin();
            iterend = m_PossibleOutputs[n].descriptors.end();
            iterend--;
            while( (*iter) != tempDescriptor ) iter++;
            delete timeOutput;
            timeOutput = NULL;
          }
          lastTime = curTime;
          curTime = 0;
        }
      }

      //dont forget to check the last slice
      if( deleteIterator )
        //delete the slice
        iter = m_PossibleOutputs[n].descriptors.erase( iter );

      if( curTime > m_PossibleOutputs[n].numberOfTimeSlices )
      //set slice and timeslice
      {
        timeOutput->numberOfSlices ++;
        if( timeOutput->numberOfTimeSlices == -1 )
          timeOutput->numberOfTimeSlices = ( curTime - m_PossibleOutputs[n].numberOfTimeSlices - 1 );
        else
          if( curTime < timeOutput->numberOfTimeSlices )
          {
            timeOutput->numberOfTimeSlices = ( curTime - m_PossibleOutputs[n].numberOfTimeSlices - 1 );
            timeOutput->differentTimeSlices = true;
          }
          else
          {
            if( curTime > timeOutput->numberOfTimeSlices )
              timeOutput->differentTimeSlices = true;
          }
      }

      if( timeOutput != NULL )
      // "close" output
      {
        m_PossibleOutputs.push_back( (*timeOutput) );
        delete timeOutput;
      }

      delete isg;
    }
    //we cleaned the current possibleOutput
    m_PossibleOutputs[n].differentTimeSlices = false;
  }
#endif
}

void mitk::ImageNumberFilter::SplitDummiVolumes()
{
#ifdef CHILI_PLUGIN_VERSION_CODE
  Vector3D spacing;
  interSliceGeometry_t* isg = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );

  for( unsigned int n = 0; n < m_PossibleOutputs.size(); n++)
  {
    if( m_PossibleOutputs[n].numberOfSlices == 1 && m_PossibleOutputs[n].numberOfTimeSlices == 0 && m_PossibleOutputs[n].differentTimeSlices == false && m_PossibleOutputs[n].dimension == 2 )
    {
      //create a "new" Output
      DifferentOutputs new2DOutput;

      new2DOutput.refferenceUID = m_PossibleOutputs[n].refferenceUID;
      new2DOutput.seriesDescription = m_PossibleOutputs[n].seriesDescription;
      new2DOutput.normale = m_PossibleOutputs[n].normale;
      new2DOutput.pixelSize = m_PossibleOutputs[n].pixelSize;
      new2DOutput.sliceSpacing = m_PossibleOutputs[n].sliceSpacing;
      new2DOutput.dimension = m_PossibleOutputs[n].dimension;
      new2DOutput.origin = m_PossibleOutputs[n].origin;
      new2DOutput.numberOfSlices = 0;
      new2DOutput.numberOfTimeSlices = 0;
      new2DOutput.differentTimeSlices = false;
      new2DOutput.descriptors.clear();
      new2DOutput.descriptors.push_back( m_PossibleOutputs[n].descriptors.front() );

      m_PossibleOutputs[n].descriptors.pop_front();
      m_PossibleOutputs[n].numberOfSlices = 0;
      m_PossibleOutputs.push_back( new2DOutput );
    }
  }
  delete isg;
#endif
}

//create the mitk::DataTreeNodes from the possible Outputs
void mitk::ImageNumberFilter::CreateNodesFromOutputs()
{
#ifdef CHILI_PLUGIN_VERSION_CODE
  m_ImageInstanceUIDs.clear();

  for( unsigned int n = 0; n < m_PossibleOutputs.size(); n++)
  {
    //check the count of slices with the numberOfSlices and numberOfTimeSlices
    if( m_PossibleOutputs[n].dimension == 2 && (unsigned int)( ( m_PossibleOutputs[n].numberOfSlices+1 ) * ( m_PossibleOutputs[n].numberOfTimeSlices+1 ) ) != m_PossibleOutputs[n].descriptors.size() )
    {
      std::cout<<"ImageNumberFilter-ERROR: For Output"<<n<<" ("<<m_PossibleOutputs[n].seriesDescription<<") calculated slicecount is not equal to the existing slices. Output closed."<<std::endl;
      continue;
    }
    if(  m_PossibleOutputs[n].differentTimeSlices == true )
    {
      std::cout<<"ImageNumberFilter-ERROR: Output"<<n<<" ("<<m_PossibleOutputs[n].seriesDescription<<") have different numbers of timeslices. Function SeperateOutputsByTime() dont work right. Output closed."<<std::endl;
      continue;
    }

    Image::Pointer resultImage = Image::New();
    std::list< std::string > ListOfUIDs;
    ListOfUIDs.clear();

    if( m_PossibleOutputs[n].dimension == 4 )
    {
      resultImage->Initialize( m_PossibleOutputs[n].descriptors.front() );
      resultImage->SetPicChannel( m_PossibleOutputs[n].descriptors.front() );

      //get ImageInstanceUID
      std::string SingleUID;
      ipPicTSV_t* missingImageTagQuery = ipPicQueryTag( m_PossibleOutputs[n].descriptors.front(), (char*)tagIMAGE_INSTANCE_UID );
      if( missingImageTagQuery )
        SingleUID = static_cast<char*>( missingImageTagQuery->value );
      else
      {
        ipPicTSV_t *dicomHeader = ipPicQueryTag( m_PossibleOutputs[n].descriptors.front(), (char*)"SOURCE HEADER" );
        void* data = NULL;
        ipUInt4_t len = 0;
        if( dicomHeader && dicomFindElement( (unsigned char*) dicomHeader->value, 0x0008, 0x0018, &data, &len ) && data != NULL )
          SingleUID = static_cast<char*>( data );
      }
      ListOfUIDs.push_back( SingleUID );
    }
    else
    {
      int slice = 0, time = 0;
      Point3D origin;
      Vector3D rightVector, downVector, origincur, originb4;
      ipPicDescriptor* header;
      header = ipPicCopyHeader( m_PossibleOutputs[n].descriptors.front(), NULL );

      //2D
      if( m_PossibleOutputs[n].numberOfSlices == 0 )
      {
        if( m_PossibleOutputs[n].numberOfTimeSlices == 0 )
        {
          header->dim = 2;
          header->n[2] = 0;
          header->n[3] = 0;
        }
        // +t
        else
        {
          header->dim = 4;
          header->n[2] = 1;
          header->n[3] = m_PossibleOutputs[n].numberOfTimeSlices + 1;
        }
      }
      //3D
      else
      {
        if( m_PossibleOutputs[n].numberOfTimeSlices == 0 )
        {
          header->dim = 3;
          header->n[2] = m_PossibleOutputs[n].numberOfSlices + 1;
          header->n[3] = 0;
        }
        // +t
        else
        {
          header->dim = 4;
          header->n[2] = m_PossibleOutputs[n].numberOfSlices + 1;
          header->n[3] = m_PossibleOutputs[n].numberOfTimeSlices + 1;
        }
      }
      interSliceGeometry_t* isg = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
      resultImage->Initialize( header, 1, -1, m_PossibleOutputs[n].numberOfSlices+1 );

      if( !pFetchSliceGeometryFromPic( m_PossibleOutputs[n].descriptors.front(), isg ) )
      {
        delete isg;
        return;
      }
      vtk2itk( isg->u, rightVector );
      vtk2itk( isg->v, downVector );
      vtk2itk( isg->o, origin );

      // its possible that a 2D-Image have no right- or down-Vector,but its not possible to initialize a [0,0,0] vector
      if( rightVector[0] == 0 && rightVector[1] == 0 && rightVector[2] == 0 )
        rightVector[0] = 1;
      if( downVector[0] == 0 && downVector[1] == 0 && downVector[2] == 0 )
        downVector[2] = -1;

      // set the timeBounds
      ScalarType timeBounds[] = {0.0, 1.0};
      // set the planeGeomtry
      PlaneGeometry::Pointer planegeometry = PlaneGeometry::New();

      //spacing
      Vector3D spacing;
      vtk2itk( isg->ps, spacing );
      if( spacing[0] == 0 && spacing[1] == 0 && spacing[2] == 0  || spacing[2] == 0.01 )
        spacing.Fill(1.0);

      //get the most counted spacing (the real spacing is needed, the saved one is rounded)
      if( m_PossibleOutputs[n].descriptors.size() > 2 && m_PossibleOutputs[n].dimension == 2 )
      {
        std::list<SpacingStruct> SpacingList;
        Vector3D tmpSpacing, origin, originb4;
        std::list< ipPicDescriptor* >::iterator iterFirst = m_PossibleOutputs[n].descriptors.begin();
        interSliceGeometry_t* isgSecond = (interSliceGeometry_t*) malloc ( sizeof(interSliceGeometry_t) );
        pFetchSliceGeometryFromPic( (*iterFirst), isgSecond );
        //first origin
        vtk2itk( isgSecond->o, originb4 );

        for( std::list< ipPicDescriptor* >::iterator iterSecond = iterFirst; iterSecond != m_PossibleOutputs[n].descriptors.end(); iterSecond++)
        {
          pFetchSliceGeometryFromPic( (*iterSecond), isgSecond );
          vtk2itk( isgSecond->o, origin );
          if( !Equal(origin, originb4 ) )
          {
            tmpSpacing = origin - originb4;
            spacing[2] = tmpSpacing.GetNorm();
            //search for spacing
            std::list<SpacingStruct>::iterator searchIter = SpacingList.begin();
            while( searchIter != SpacingList.end() )
            {
              if( searchIter->spacing == spacing )
              {
                searchIter->count++;
                break;
              }
              else
                searchIter++;
            }
            //if not exist, create new entry
            if( searchIter == SpacingList.end() )
            {
              SpacingStruct newElement;
              newElement.spacing = spacing;
              newElement.count = 1;
              SpacingList.push_back( newElement );
            }
            originb4 = origin;
          }
        }
        //get maximum spacing
        int count = 0;
        for( std::list<SpacingStruct>::iterator searchIter = SpacingList.begin(); searchIter != SpacingList.end(); searchIter++ )
        {
          if( searchIter->count > count )
          {
            spacing = searchIter->spacing;
            count = searchIter->count;
          }
        }
        delete isgSecond;
      }

      planegeometry->InitializeStandardPlane( resultImage->GetDimension(0), resultImage->GetDimension(1), rightVector, downVector, &spacing );
      planegeometry->SetOrigin( origin );
      planegeometry->SetFrameOfReferenceID( FrameOfReferenceUIDManager::AddFrameOfReferenceUID( m_PossibleOutputs[n].refferenceUID.c_str() ) );
      planegeometry->SetTimeBounds( timeBounds );
      // slicedGeometry
      SlicedGeometry3D::Pointer slicedGeometry = SlicedGeometry3D::New();
      slicedGeometry->InitializeEvenlySpaced( planegeometry, resultImage->GetDimension(2) );
      // timeSlicedGeometry
      TimeSlicedGeometry::Pointer timeSliceGeometry = TimeSlicedGeometry::New();
      timeSliceGeometry->InitializeEvenlyTimed( slicedGeometry, resultImage->GetDimension(3) );
      timeSliceGeometry->TransferItkToVtkTransform();
      // Image->SetGeometry
      resultImage->SetGeometry( timeSliceGeometry );

      // add the slices to the created mitk::Image
      for( std::list< ipPicDescriptor* >::iterator iter = m_PossibleOutputs[n].descriptors.begin(); iter != m_PossibleOutputs[n].descriptors.end(); iter++)
      {
        //get ImageInstanceUID
        std::string SingleUID;
        ipPicTSV_t* missingImageTagQuery = ipPicQueryTag( (*iter), (char*)tagIMAGE_INSTANCE_UID );
        if( missingImageTagQuery )
          SingleUID = static_cast<char*>( missingImageTagQuery->value );
        else
        {
          ipPicTSV_t *dicomHeader = ipPicQueryTag( (*iter), (char*)"SOURCE HEADER" );
          void* data = NULL;
          ipUInt4_t len = 0;
          if( dicomHeader && dicomFindElement( (unsigned char*) dicomHeader->value, 0x0008, 0x0018, &data, &len ) && data != NULL )
            SingleUID = static_cast<char*>( data );
        }
        ListOfUIDs.push_back( SingleUID );

        //add to mitk::Image
        if( m_PossibleOutputs[n].dimension == 3 )
          resultImage->SetPicVolume( (*iter), time );
        else
          resultImage->SetPicSlice( (*iter), slice, time );

        if( time < m_PossibleOutputs[n].numberOfTimeSlices )
          time ++;
        else
        {
          time = 0;
          slice ++;
        }
      }
      delete isg;
    }

    // if all okay create a node, add the NumberOfSlices, NumberOfTimeSlices, SeriesOID, name, data and all pic-tags as properties
    if( resultImage->IsInitialized() && resultImage.IsNotNull() )
    {
      DataTreeNode::Pointer node = mitk::DataTreeNode::New();
      node->SetData( resultImage );
      DataTreeNodeFactory::SetDefaultImageProperties( node );

      if( m_PossibleOutputs[n].seriesDescription == "" )
        m_PossibleOutputs[n].seriesDescription = "no SeriesDescription";
      node->SetProperty( "name", new StringProperty( m_PossibleOutputs[n].seriesDescription ) );
      node->SetProperty( "NumberOfSlices", new IntProperty( m_PossibleOutputs[n].numberOfSlices+1 ) );
      node->SetProperty( "NumberOfTimeSlices", new IntProperty( m_PossibleOutputs[n].numberOfTimeSlices+1 ) );
      if( m_SeriesOID != "" )
        node->SetProperty( "SeriesOID", new StringProperty( m_SeriesOID ) );

      mitk::PropertyList::Pointer tempPropertyList = CreatePropertyListFromPicTags( m_PossibleOutputs[n].descriptors.front() );
      for( mitk::PropertyList::PropertyMap::const_iterator iter = tempPropertyList->GetMap()->begin(); iter != tempPropertyList->GetMap()->end(); iter++ )
      {
        node->SetProperty( iter->first.c_str(), iter->second.first );
      }

      m_Output.push_back( node );
      m_ImageInstanceUIDs.push_back( ListOfUIDs );
    }
  }
#endif
}

std::vector< std::list< std::string > > mitk::ImageNumberFilter::GetImageInstanceUIDs()
{
  return m_ImageInstanceUIDs;
}

const mitk::PropertyList::Pointer mitk::ImageNumberFilter::CreatePropertyListFromPicTags( ipPicDescriptor* imageToExtractTagsFrom )
{
  if( !imageToExtractTagsFrom || !imageToExtractTagsFrom->info || !imageToExtractTagsFrom->info->tags_head )
    return NULL;

  PropertyList::Pointer resultPropertyList = PropertyList::New();
  _ipPicTagsElement_t* currentTagNode = imageToExtractTagsFrom->info->tags_head;

  // Extract ALL tags from the PIC header
  while (currentTagNode)
  {
    ipPicTSV_t* currentTag = currentTagNode->tsv;

    std::string propertyName = "CHILI: ";
    propertyName += currentTag->tag;

    //The currentTag->tag ends with a lot of ' ', so you find nothing if you search for the properties.
    while( propertyName[ propertyName.length() -1 ] == ' ' )
      propertyName.erase( propertyName.length() -1 );

    switch( currentTag->type )
    {
      case ipPicASCII:
      {
        resultPropertyList->SetProperty( propertyName.c_str(), new mitk::StringProperty( static_cast<char*>( currentTag->value ) ) );
        break;
      }
      case ipPicInt:
      {
        resultPropertyList->SetProperty( propertyName.c_str(), new mitk::IntProperty( *static_cast<int*>( currentTag->value ) ) );
        break;
      }
      case ipPicUInt:
      {
        resultPropertyList->SetProperty( propertyName.c_str(), new mitk::IntProperty( (int)*( (char*)( currentTag->value ) ) ) );
        break;
      }
      default:  //ipPicUnknown, ipPicBool, ipPicFloat, ipPicNonUniform, ipPicTSV, _ipPicTypeMax
      {
        //every PicDescriptor have the following tags wich not get imported, but they are not so important that we have to throw messages
        if( propertyName != "CHILI: PIXEL SIZE" && propertyName != "CHILI: REAL PIXEL SIZE" && propertyName != "CHILI: ISG" && propertyName != "CHILI: SOURCE HEADER" && propertyName != "CHILI: PIXEL SPACING" )
        {
          std::cout << "WARNING: Type of PIC-Tag '" << currentTag->type << "'( " << propertyName << " ) not handled in mitkImageNumberFilter." << std::endl;
        }
        break;
      }
    }
    // proceed to the next tag
    currentTagNode = currentTagNode->next;
  }
  return resultPropertyList;
}

// show all PossibleOutputs and the descriptors
void mitk::ImageNumberFilter::DebugOutput()
{
  for( unsigned int n = 0; n < m_PossibleOutputs.size(); n++)
  {
    //the Head of all ipPicDescriptors
    std::cout << "-----------" << std::endl;
    std::cout << "ImageNumberFilter-Output" << n << std::endl;
    std::cout << "ReferenceUID:" << m_PossibleOutputs[n].refferenceUID << std::endl;
    std::cout << "SeriesDescription:" << m_PossibleOutputs[n].seriesDescription << std::endl;
    std::cout << "Normale:" << m_PossibleOutputs[n].normale << std::endl;
    std::cout << "PixelSize:" << m_PossibleOutputs[n].pixelSize << std::endl;
    std::cout << "SliceSpacing:" << m_PossibleOutputs[n].sliceSpacing << std::endl;
    std::cout << "Dimension:" << m_PossibleOutputs[n].dimension << std::endl;
    std::cout << "Origin:" << m_PossibleOutputs[n].origin << std::endl;
    std::cout << "NumberOfSlices:" << m_PossibleOutputs[n].numberOfSlices << std::endl;
    std::cout << "NumberOfTimeSlices:" << m_PossibleOutputs[n].numberOfTimeSlices << std::endl;
    std::cout << "DifferentTimeSlices (bool):" << m_PossibleOutputs[n].differentTimeSlices << std::endl;
    std::cout << "-----------" << std::endl;

    //every single descriptor by the ImageNumber
    for( std::list< ipPicDescriptor* >::iterator it = m_PossibleOutputs[n].descriptors.begin(); it != m_PossibleOutputs[n].descriptors.end(); it++ )
    {
      int imageNumber = 0;
      ipPicTSV_t *tsv;
      void* data = NULL;
      ipUInt4_t len = 0;
      tsv = ipPicQueryTag( (*it), (char*)"SOURCE HEADER" );
      if( tsv )
      {
        bool ok = dicomFindElement( (unsigned char*) tsv->value, 0x0020, 0x0013, &data, &len );
        if( ok )
          sscanf( (char *) data, "%d", &imageNumber );
      }
      if( imageNumber == 0)
      {
        ipPicTSV_t* imagenumberTag = ipPicQueryTag( (*it), (char*)tagIMAGE_NUMBER );
        if( imagenumberTag && imagenumberTag->type == ipPicInt )
          imageNumber = *( (int*)(imagenumberTag->value) );
      }
      std::cout<<"Image: "<<imageNumber<<std::endl;
    }
    std::cout<<""<<std::endl;
  }
}
